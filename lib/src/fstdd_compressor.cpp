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
                               size_t block_size, size_t compress_level,
                               size_t worker_num, bool opt_verbose) {
  thread reader([&]() { read_files(data_paths, header, block_size); });
  if (worker_num == 0) {
    worker_num = get_optimal_thread_num(TaskType::CPU_INTENSIVE);
  }
  vector<thread> workers;
  for (unsigned int i = 0; i < worker_num; ++i) {
    workers.emplace_back([&]() { compress_worker(compress_level); });
  }
  thread writer([&]() { write_output(out, header, compress_level); });
  reader.join();
  for (auto &worker : workers) {
    worker.join();
  }
  // mark compress finished
  {
    unique_lock<mutex> lock(res_mtx);
    compress_finished = true;
    res_cv.notify_all();
  }
  writer.join();

  if (!success) {
    LOG_ERROR("Compress failed.");
    return false;
  }
  LOG_DEBUG("Compress done.");
  return success;
}

vector<char> FstddCompressor::zstd_compress_block(const char *data, size_t size,
                                                  int level) {
  size_t dst_cap = ZSTD_compressBound(size);
  vector<char> dst(dst_cap);
  size_t dst_size = ZSTD_compress(dst.data(), dst_cap, data, size, level);
  if (ZSTD_isError(dst_size)) {
    success = false;
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

// -------------------- read files with single thread --------------------
void FstddCompressor::read_files(const std::vector<std::string> &data_paths,
                                 DdJsonHeader &header, size_t block_size) {
  vector<char> disk_buf(disk_read_size);
  size_t buf_size = 0;
  uint64_t block_index = 0;
  vector<pair<string, size_t>> files_paths = recursive_directory(data_paths);
  size_t record = 0;
  DyBlockProgBars dy_bars;
  auto refresh_bar =
      dy_bars.push_back(files_paths.size(), "Compressing files:");
  for (size_t i = 0; i < files_paths.size(); ++i) {
    string key = files_paths[i].first;
    fs::path file_path =
        fs::path(data_paths[files_paths[i].second]) / fs::path(key);
    ifstream ifs(file_path.string(), ios::binary);
    if (!ifs) {
      LOG_ERROR("Failed to open file: {}", file_path.string());
      continue;
    }
    if (!success) { return; }
    size_t start_count = buf_size / block_size;
    uint64_t start_offset = buf_size % block_size;
    uint64_t start_block_index = block_index + start_count;
    while (true) {
      if (!success) { return; }
      size_t expect_read_size = disk_read_size - buf_size;
      ifs.read(disk_buf.data() + buf_size, expect_read_size);
      streamsize bytes_read = ifs.gcount();
      buf_size += bytes_read;
      if (static_cast<size_t>(bytes_read) < expect_read_size) {
        size_t end_count = buf_size / block_size;
        uint64_t end_offset = buf_size % block_size;
        uint64_t end_block_index = block_index + end_count;
        LOG_DEBUG("{} : {}, {}, {}, {}", key, start_block_index, start_offset,
                  end_block_index, end_offset);
        index_record.emplace_back(
            std::move(key), ValueBlockPosIndex(start_block_index, start_offset,
                                               end_block_index, end_offset));
        break;
      }

      // split disk_buf into blocks
      size_t offset = 0;
      while (offset < buf_size) {
        size_t current_block_size = block_size;

        CompressTask task;
        task.src_data.assign(disk_buf.begin() + offset,
                             disk_buf.begin() + offset + current_block_size);
        task.index = task_seq.fetch_add(1);

        // wait for task queue to reduce memory usage
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
    refresh_bar(i);
  }

  // last block
  size_t offset = 0;
  while (offset < buf_size) {
    size_t current_block_size = min(block_size, buf_size - offset);

    CompressTask task;
    task.src_data.assign(disk_buf.begin() + offset,
                         disk_buf.begin() + offset + current_block_size);
    task.index = task_seq.fetch_add(1);

    unique_lock<mutex> lock(task_mtx);
    task_cv.wait(lock, [&] { return task_queue.size() < max_queue_size; });

    task_queue.push(std::move(task));
    task_cv.notify_one();

    offset += current_block_size;
  }

  // mark read finished
  unique_lock<mutex> lock(task_mtx);
  header["meta"]["Record"] = record;
  header["comp_blocks"]["block_size"] = block_size;
  read_finished = true;
  task_cv.notify_all();
}

// -------------------- compress files with multiple threads -----------------
void FstddCompressor::compress_worker(size_t compress_level) {
  while (true) {
    CompressTask task;
    {
      unique_lock<mutex> lock(task_mtx);
      task_cv.wait(lock, [&] { return !task_queue.empty() || read_finished; });

      if (task_queue.empty() && read_finished) { break; }

      task = std::move(task_queue.front());
      task_queue.pop();
      task_cv.notify_one(); // notify read thread to continue
    }

    CompressResult result;
    result.dst_data = zstd_compress_block(task.src_data.data(),
                                          task.src_data.size(), compress_level);
    if (!success) { return; }
    result.index = task.index;
    {
      unique_lock<mutex> lock(res_mtx);
      result_queue.push(std::move(result));
      res_cv.notify_one();
    }
  }
}

// -------------------- write output with single thread --------------------
void FstddCompressor::write_output(std::ostream &out, DdJsonHeader &header,
                                   size_t compress_level) {
  size_t expected_index = 0;
  // (Key: index, Value: Compressed Data)
  unordered_map<size_t, vector<char>> fallback_buffer;
  vector<uint64_t> block_indexes;
  uint64_t block_offset = 0;
  uint64_t total_block_size = 0;

  while (true) {
    if (!success) { return; }
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
    // if new data is received, put it into fallback buffer
    if (!result.dst_data.empty() || result.index == expected_index) {
      fallback_buffer[result.index] = std::move(result.dst_data);
    }
    // write compressed data to output with strict order
    while (fallback_buffer.count(expected_index)) {
      if (!success) { return; }
      auto &data = fallback_buffer[expected_index];
      uint64_t comp_block_size = static_cast<uint64_t>(data.size());
      block_indexes.push_back((block_offset << 24) | comp_block_size);
      block_offset += comp_block_size;

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
      header["hash_buckets"]["offset"].get<size_t>() + bucket_data_size;

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
  if (!compress_to_buffer(keys_buf.data(), keys_buf.size(), dst,
                          compress_level)) {
    success = false;
    return;
  }
  out.write(dst.data(), dst.size());
  header["keys"]["offset"] = header["hash_index"]["offset"].get<size_t>() +
                             bucket_size * sizeof(uint64_t);
  header["keys"]["compress_level"] = compress_level;
  header["keys"]["original_size"] = keys_size;
  header["keys"]["compressed_size"] = dst.size();
}

} // namespace fstd
