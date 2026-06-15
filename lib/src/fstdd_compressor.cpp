#include <atomic>
#include <condition_variable>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include <zstd.h>

#include <fstd/common.h>
#include <fstd/fstdd_compressor.h>
#include <fstd/hash_index.h>
#include <fstd/logger.h>
#include <fstd/thread_pool.h>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using namespace std;

namespace fstd {

std::ostream &operator<<(std::ostream &os, const ValueBlockPosIndex &vbp_idx) {
  os << " start_index: " << vbp_idx.get_start_index()
     << " start_offset: " << vbp_idx.get_start_offset()
     << " end_index: " << vbp_idx.get_end_index()
     << " end_offset: " << vbp_idx.get_end_offset();
  return os;
}

bool FstddCompressor::compress(const std::vector<std::string> &data_paths,
                               std::ofstream &out, DdJsonHeader &header,
                               size_t worker_num, bool opt_verbose) {

  // 1. 启动读取线程
  thread reader([&]() { read_files(data_paths, header); });

  if (worker_num == 0) {
    worker_num = get_optimal_thread_num(TaskType::CPU_INTENSIVE);
  }
  vector<thread> workers;
  for (unsigned int i = 0; i < worker_num; ++i) {
    workers.emplace_back([&]() { compress_worker(); });
  }

  // 3. 启动落盘写入线程
  thread writer([&]() { write_output(out, header); });

  // 4. 等待读取完毕，回收工作线程
  reader.join();

  for (auto &worker : workers) {
    worker.join();
  }

  // 5. 标记压缩完全结束，通知并回收写入线程
  {
    unique_lock<mutex> lock(res_mtx);
    compress_finished = true;
    res_cv.notify_all();
  }
  writer.join();

  LOG_INFO("Compress done.");
  return success;
}

// -------------------- 工具：zstd 单块压缩 --------------------
vector<char> FstddCompressor::zstd_compress_block(const char *data, size_t size,
                                                  int level) {
  size_t dst_cap = ZSTD_compressBound(size);
  vector<char> dst(dst_cap);
  size_t dst_size = ZSTD_compress(dst.data(), dst_cap, data, size, level);
  if (ZSTD_isError(dst_size)) {
    LOG_ERROR("zstd compress error: {}", ZSTD_getErrorName(dst_size));
    return {};
  }
  dst.resize(dst_size);
  return dst;
}

std::vector<std::pair<std::string, size_t>>
FstddCompressor::recursive_directory(
    const std::vector<std::string> &data_paths) {
  std::vector<std::pair<std::string, size_t>> files_paths;
  for (size_t i = 0; i < data_paths.size(); ++i) {
    const string &data_path = data_paths[i];
    for (const auto &entry : fs::recursive_directory_iterator(data_path)) {
      if (!entry.is_regular_file()) { continue; }
      std::string file =
          std::filesystem::relative(entry.path(), data_path).generic_string();
      files_paths.emplace_back(std::move(file), i);
    }
  }
  return files_paths;
}

// -------------------- 线程1：单线程顺序读取 & 切块 --------------------
void FstddCompressor::read_files(const std::vector<std::string> &data_paths,
                                 DdJsonHeader &header) {
  vector<char> disk_buf(disk_read_size);
  size_t buf_size = 0;
  uint64_t block_index = 0;
  vector<pair<string, size_t>> files_paths = recursive_directory(data_paths);
  size_t record = 0;
  for (size_t i = 0; i < files_paths.size(); ++i) {
    string key = files_paths[i].first;
    fs::path file_path =
        fs::path(data_paths[files_paths[i].second]) / fs::path(key);
    ifstream ifs(file_path.string(), ios::binary);
    if (!ifs) {
      LOG_ERROR("Failed to open file: {}", file_path.string());
      continue;
    }
    size_t start_count = buf_size / zstd_block_size;
    uint64_t start_offset = buf_size % zstd_block_size;
    uint64_t start_block_index = block_index + start_count;
    while (true) {
      size_t expect_read_size = disk_read_size - buf_size;
      ifs.read(disk_buf.data() + buf_size, expect_read_size);
      streamsize bytes_read = ifs.gcount();
      buf_size += bytes_read;
      if (static_cast<size_t>(bytes_read) < expect_read_size) {

        size_t end_count = buf_size / zstd_block_size;
        uint64_t end_offset = buf_size % zstd_block_size;
        uint64_t end_block_index = block_index + end_count;
        LOG_DEBUG("{} : {}, {}, {}, {}", key, start_block_index, start_offset,
                  end_block_index, end_offset);
        index_record.emplace_back(
            std::move(key), ValueBlockPosIndex(start_block_index, start_offset,
                                               end_block_index, end_offset));
        break;
      }

      // 将 512KB 的磁盘数据切碎为 128KB 的 ZSTD 最优块提交
      size_t offset = 0;
      while (offset < buf_size) {
        size_t current_block_size = zstd_block_size;

        CompressTask task;
        task.src_data.assign(disk_buf.begin() + offset,
                             disk_buf.begin() + offset + current_block_size);
        task.index = task_seq.fetch_add(1);

        // 队列控流：防止内存无限上涨
        unique_lock<mutex> lock(task_mtx);
        task_cv.wait(lock, [&] { return task_queue.size() < max_queue_size; });

        task_queue.push(std::move(task));
        task_cv.notify_one();
        block_index += 1;
        offset += current_block_size;
      }
      buf_size = 0;
    }
    record += 1;
  }

  size_t offset = 0;
  while (offset < buf_size) {
    size_t current_block_size = min(zstd_block_size, buf_size - offset);

    CompressTask task;
    task.src_data.assign(disk_buf.begin() + offset,
                         disk_buf.begin() + offset + current_block_size);
    task.index = task_seq.fetch_add(1);

    // 队列控流：防止内存无限上涨
    unique_lock<mutex> lock(task_mtx);
    task_cv.wait(lock, [&] { return task_queue.size() < max_queue_size; });

    task_queue.push(std::move(task));
    task_cv.notify_one();

    offset += current_block_size;
  }

  // 标记读取流结束
  unique_lock<mutex> lock(task_mtx);
  header["meta"]["Record"] = record;
  read_finished = true;
  task_cv.notify_all();
}

// -------------------- 线程池：多线程并行压缩 --------------------
void FstddCompressor::compress_worker() {
  while (true) {
    CompressTask task;
    {
      unique_lock<mutex> lock(task_mtx);
      task_cv.wait(lock, [&] { return !task_queue.empty() || read_finished; });

      if (task_queue.empty() && read_finished) {
        break; // 没有任务且读取完毕，退出线程
      }

      task = std::move(task_queue.front());
      task_queue.pop();
      task_cv.notify_one(); // 通知读取线程队列有空位了
    }

    // 执行 CPU 密集的压缩计算
    CompressResult result;
    result.dst_data = zstd_compress_block(task.src_data.data(),
                                          task.src_data.size(), zstd_level);
    result.index = task.index;

    {
      unique_lock<mutex> lock(res_mtx);
      result_queue.push(std::move(result));
      res_cv.notify_one();
    }
  }
}

// -------------------- 线程3：单线程严格顺序写入 --------------------
void FstddCompressor::write_output(std::ostream &out, DdJsonHeader &header) {
  size_t expected_index = 0;
  // 使用缓存区存放乱序到达的压缩块 (Key: index, Value: Compressed Data)
  unordered_map<size_t, vector<char>> fallback_buffer;
  vector<uint64_t> block_indexes;
  uint64_t block_offset = 0;
  uint64_t total_block_size = 0;

  while (true) {
    CompressResult result;
    {
      unique_lock<mutex> lock(res_mtx);
      res_cv.wait(lock,
                  [&] { return !result_queue.empty() || compress_finished; });

      if (result_queue.empty() && compress_finished &&
          fallback_buffer.empty()) {
        break;
      }

      if (!result_queue.empty()) {
        result = std::move(result_queue.front());
        result_queue.pop();
      }
    }

    // 如果取到了新数据，放入缓存区
    if (!result.dst_data.empty() || result.index == expected_index) {
      fallback_buffer[result.index] = std::move(result.dst_data);
    }

    // 核心：严格按序号循环写入，直到缺憾后续块为止
    while (fallback_buffer.count(expected_index)) {
      auto &data = fallback_buffer[expected_index];

      // 写入元数据头 (数据流块大小)，以便后续解压程序定位块边界
      uint64_t block_size = static_cast<uint64_t>(data.size());

      block_indexes.push_back((block_offset << 24) | block_size);
      block_offset += block_size;

      // 写入压缩数据主体
      out.write(data.data(), data.size());
      total_block_size += data.size();

      fallback_buffer.erase(expected_index);
      expected_index++;
    }
  }

  header["comp_blocks"]["offset"] = 0;
  out.write(reinterpret_cast<const char *>(block_indexes.data()),
            block_indexes.size() * sizeof(uint64_t));
  header["block_index"]["offset"] = total_block_size;
  header["hash_buckets"]["offset"] =
      total_block_size + block_indexes.size() * sizeof(uint64_t);
  set<size_t> dup_bucket_idxes;
  auto [bucket_size, bucket_data_size] =
      write_hash_index(out, index_record, dup_bucket_idxes);
  header["hash_index"]["dup_idxes"] =
      vector<size_t>(dup_bucket_idxes.begin(), dup_bucket_idxes.end());
  header["hash_index"]["bucket_size"] = bucket_size;
  header["hash_index"]["offset"] =
      static_cast<size_t>(header["hash_buckets"]["offset"]) + bucket_data_size;

  vector<size_t> sorted_idxes = sort_indexes(index_record);
  size_t keys_size = 0;
  for (auto i : sorted_idxes) {
    const string &key = index_record[i].first;
    keys_size += key.size() + 1;
  }
  vector<char> keys_buf(keys_size);
  size_t offset = 0;
  for (auto i : sorted_idxes) {
    const string &key = index_record[i].first;
    memcpy(keys_buf.data() + offset, key.c_str(), key.size() + 1);
    offset += key.size() + 1;
  }

  std::vector<char> dst;
  if (!compress_to_buffer(keys_buf.data(), keys_buf.size(), dst, zstd_level)) {
    success = false;
    return;
  }
  out.write(dst.data(), dst.size());
  header["keys"]["offset"] =
      static_cast<size_t>(header["hash_index"]["offset"]) +
      bucket_size * sizeof(uint64_t);
  header["keys"]["compress_level"] = zstd_level;
  header["keys"]["original_size"] = keys_size;
  header["keys"]["compressed_size"] = dst.size();
}

} // namespace fstd
