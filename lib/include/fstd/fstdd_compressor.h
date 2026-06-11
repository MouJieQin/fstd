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

#include <fstd/fstlib.h>
// #include <fstd/logger.h>

namespace fs = std::filesystem;
using namespace std;

namespace fstd {

// ===================== 【核心配置】完全按你的最优方案 =====================
const size_t disk_read_size = 512 * 1024;  // 磁盘读取：512kb（最快）
const size_t zstd_block_size = 128 * 1024; // zstd 压缩块：128kb（最优）
const int zstd_level = 3;        // 压缩级别（3=速度优先，5=压缩率优先）
const size_t max_queue_size = 8; // 队列缓冲（内存友好）
// =========================================================================

struct ValueBlockPosIndex {
  ValueBlockPosIndex() = default;
  ValueBlockPosIndex(uint64_t start_index, uint64_t start_offset,
                     uint64_t end_index, uint64_t end_offset)
      : start_code((start_index << 32) | start_offset),
        end_code((end_index << 32) | end_offset) {}
  uint32_t get_start_index() const { return start_code >> 32; }
  uint32_t get_start_offset() const { return start_code & 0xFFFFFFFF; }
  uint32_t get_end_index() const { return end_code >> 32; }
  uint32_t get_end_offset() const { return end_code & 0xFFFFFFFF; }
  uint64_t start_code;
  uint64_t end_code;
};

std::ostream &operator<<(std::ostream &os, const ValueBlockPosIndex &vbp_idx) {
  os << " start_index: " << vbp_idx.get_start_index()
     << " start_offset: " << vbp_idx.get_start_offset()
     << " end_index: " << vbp_idx.get_end_index()
     << " end_offset: " << vbp_idx.get_end_offset();
  return os;
}

struct compresstask {
  vector<char> src_data;
  size_t index;
};

struct compressresult {
  vector<char> dst_data;
  size_t index;
};

class FstddCompressor {
public:
  int compress(const string &data_path, const string &output_path) {

    vector<string> data_paths{data_path};

    // 1. 启动读取线程
    thread reader([&]() { read_files(data_paths); });

    // 2. 启动 CPU 核心数匹配的压缩线程池
    unsigned int cpu_threads = thread::hardware_concurrency();
    if (cpu_threads == 0) cpu_threads = 4;
    vector<thread> workers;
    for (unsigned int i = 0; i < cpu_threads; ++i) {
      workers.emplace_back([&]() { compress_worker(); });
    }

    // 3. 启动落盘写入线程
    thread writer([&]() { write_output(output_path); });

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

    cout << "多线程串行无损压缩完成！" << endl;
    return 0;
  }

  bool extract(const string &key, const std::string &dst_dir = ".") {
    return extract_impl(key, dst_dir);
  }

private:
  // -------------------- 工具：zstd 单块压缩 --------------------
  vector<char> zstd_compress_block(const char *data, size_t size, int level) {
    size_t dst_cap = ZSTD_compressBound(size);
    vector<char> dst(dst_cap);
    size_t dst_size = ZSTD_compress(dst.data(), dst_cap, data, size, level);
    if (ZSTD_isError(dst_size)) {
      cerr << "zstd 压缩错误: " << ZSTD_getErrorName(dst_size) << endl;
      return {};
    }
    dst.resize(dst_size);
    return dst;
  }

  // -------------------- 线程1：单线程顺序读取 & 切块 --------------------
  void read_files(const std::vector<std::string> &data_paths) {
    vector<char> disk_buf(disk_read_size);
    size_t buf_size = 0;
    uint64_t block_index = 0;
    vector<string> files_paths;
    for (const string &data_path : data_paths) {
      for (const auto &entry : fs::recursive_directory_iterator(data_path)) {
        if (!entry.is_regular_file()) { continue; }
        string file = entry.path().string();
        // std::string file =
        //     std::filesystem::relative(entry.path(), data_path).string();
        // 统一路径分隔符为 '/'，避免跨平台解压问题
        std::replace(file.begin(), file.end(), '\\', '/');
        files_paths.emplace_back(std::move(file));
      }
    }

    for (size_t i = 0; i < files_paths.size(); ++i) {
      string &file = files_paths[i];
      ifstream ifs(file, ios::binary);
      if (!ifs) {
        cerr << "无法打开文件: " << file << endl;
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
        if (bytes_read < expect_read_size) {

          size_t end_count = buf_size / zstd_block_size;
          uint64_t end_offset = buf_size % zstd_block_size;
          uint64_t end_block_index = block_index + end_count;
          cout << file << " : " << start_block_index << "," << start_offset
               << "," << end_block_index << "," << end_offset << "\n";
          index_record.emplace_back(
              std::move(file),
              ValueBlockPosIndex(start_block_index, start_offset,
                                 end_block_index, end_offset));
          break;
        }

        // 将 512KB 的磁盘数据切碎为 128KB 的 ZSTD 最优块提交
        size_t offset = 0;
        while (offset < buf_size) {
          size_t current_block_size = zstd_block_size;

          compresstask task;
          task.src_data.assign(disk_buf.begin() + offset,
                               disk_buf.begin() + offset + current_block_size);
          task.index = task_seq.fetch_add(1);

          // 队列控流：防止内存无限上涨
          unique_lock<mutex> lock(task_mtx);
          task_cv.wait(lock,
                       [&] { return task_queue.size() < max_queue_size; });

          task_queue.push(std::move(task));
          task_cv.notify_one();
          block_index += 1;
          offset += current_block_size;
        }
        buf_size = 0;
      }
    }

    size_t offset = 0;
    while (offset < buf_size) {
      size_t current_block_size = min(zstd_block_size, buf_size - offset);

      compresstask task;
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
    read_finished = true;
    task_cv.notify_all();
  }

  // -------------------- 线程池：多线程并行压缩 --------------------
  void compress_worker() {
    while (true) {
      compresstask task;
      {
        unique_lock<mutex> lock(task_mtx);
        task_cv.wait(lock,
                     [&] { return !task_queue.empty() || read_finished; });

        if (task_queue.empty() && read_finished) {
          break; // 没有任务且读取完毕，退出线程
        }

        task = std::move(task_queue.front());
        task_queue.pop();
        task_cv.notify_one(); // 通知读取线程队列有空位了
      }

      // 执行 CPU 密集的压缩计算
      compressresult result;
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
  void write_output(const string &output_filename) {
    ofstream ofs(output_filename, ios::binary);
    if (!ofs) {
      cerr << "无法创建输出文件: " << output_filename << endl;
      return;
    }

    size_t expected_index = 0;
    // 使用缓存区存放乱序到达的压缩块 (Key: index, Value: Compressed Data)
    unordered_map<size_t, vector<char>> fallback_buffer;
    vector<uint64_t> block_indexes;
    uint64_t block_offset = 0;

    while (true) {
      compressresult result;
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
        // static_assert(block_size<=65536);
        // std::cout << "block_size: " << block_size << endl;
        // if (block_size > 65536) {
        //   std::cerr << "block_size: " << block_size << " > 65536" << endl;
        // }

        block_indexes.push_back((block_offset << 24) | block_size);
        block_offset += block_size;
        // ofs.write(reinterpret_cast<const char *>(&block_size),
        //           sizeof(block_size));

        // 写入压缩数据主体
        ofs.write(data.data(), data.size());

        fallback_buffer.erase(expected_index);
        expected_index++;
      }
    }

    ofstream out("block_index.bin");
    out.write(reinterpret_cast<const char *>(block_indexes.data()),
              block_indexes.size() * sizeof(uint64_t));

    write_hash_index(index_record);
  }

  bool extract_impl(const string &key, const string &dst_dir) {
    ValueBlockPosIndex vbp_idx;
    if (!readh(key, vbp_idx)) { cout << "Not found the key: " << key << endl; }
    uint32_t start_index = vbp_idx.get_start_index();
    uint32_t start_offset = vbp_idx.get_start_offset();
    uint32_t end_index = vbp_idx.get_end_index();
    uint32_t end_offset = vbp_idx.get_end_offset();
    ifstream in("block_index.bin", ios::binary);
    if (!in) {
      std::cout << "open error" << std::endl;
      return false;
    }

    vector<uint64_t> block_indexes(end_index - start_index + 1);
    in.seekg(start_index * sizeof(uint64_t));
    in.read(reinterpret_cast<char *>(block_indexes.data()),
            sizeof(uint64_t) * block_indexes.size());

    ifstream bin("block.bin", ios::binary);
    if (!bin) {
      std::cout << "open error" << std::endl;
      return false;
    }
    vector<char> comp_blocks;
    size_t step = 4;

    string rel_path(key);
    std::replace(rel_path.begin(), rel_path.end(), '/', '_');
    const string file_path = dst_dir + "/" + rel_path;
    ofstream out(file_path, std::ios::binary);
    if (!out) {
      std::cout << "open error" << std::endl;
      return false;
    }
    std::vector<char> decomp_buf(zstd_block_size);
    for (size_t block_idx = 0; block_idx < block_indexes.size(); ++block_idx) {
      size_t idx = block_idx;
      uint64_t start_block_offset = block_indexes[block_idx] >> 24;
      if (block_idx + step >= block_indexes.size()) {
        step = block_indexes.size() - 1 - block_idx;
      }
      block_idx += step;
      uint64_t end_block_offset = block_indexes[block_idx] >> 24;
      uint64_t end_block_size = block_indexes[block_idx] & 0xFFFFFF;
      uint64_t total_block_size =
          end_block_offset - start_block_offset + end_block_size;
      comp_blocks.resize(total_block_size);
      bin.seekg(start_block_offset);
      bin.read(comp_blocks.data(), comp_blocks.size());
      size_t buf_offset = 0;
      for (; idx <= block_idx; ++idx) {
        size_t block_size = block_indexes[idx] & 0xFFFFFF;
        size_t decomp_size =
            ZSTD_decompress(decomp_buf.data(), zstd_block_size,
                            comp_blocks.data() + buf_offset, block_size);
        if (ZSTD_isError(decomp_size)) {
          cerr << "解压失败: " << ZSTD_getErrorName(decomp_size) << endl;
          return false;
        }
        size_t offset = 0;
        size_t size = decomp_size;
        if (idx == 0) {
          offset = start_offset;
          size = decomp_size - start_offset;
        }
        if (idx == block_indexes.size() - 1) { size = end_offset - offset; }
        out.write(decomp_buf.data() + offset, size);
        buf_offset += block_size;
      }
    }
    return true;
  }

  template <typename VT>
  inline int
  write_hash_index(const std::vector<std::pair<std::string, VT>> &key_value) {
    uint32_t buket_size = 8;
    if (key_value.size() > 64) { buket_size = key_value.size() / 8; }
    vector<uint64_t> hash_codes;
    vector<size_t> hash_bucket_count(buket_size, 0);
    vector<vector<size_t>> hash_bucket_map(buket_size, vector<size_t>());
    for (size_t i = 0; i < key_value.size(); ++i) {
      uint64_t hash_code = fst::MurmurHash64B(key_value[i].first.c_str(),
                                              key_value[i].first.size(), 0);
      hash_codes.push_back(hash_code);
      size_t index = hash_code % buket_size;
      hash_bucket_count[index] += 1;
      hash_bucket_map[index].push_back(i);
    }
    size_t max_count = 0;
    for (auto c : hash_bucket_count) {
      if (c > max_count) { max_count = c; }
    }
    vector<size_t> counts(max_count + 1, 0);
    for (auto c : hash_bucket_count) {
      counts[c] += 1;
    }
    cout << "buket size: " << buket_size << "\n"
         << "file num: " << key_value.size() << std::endl;
    for (size_t i = 0; i < counts.size(); ++i) {
      std::cout << i << " : " << counts[i] << "\n";
    }
    std::cout << std::endl;

    ostringstream oss(std::ios::binary);
    uint32_t pos = 0;
    uint32_t b_size = 0;
    uint32_t offset = 0;
    vector<uint64_t> hash_buket(buket_size, 0);
    for (size_t i = 0; i < buket_size; ++i) {
      b_size = 0;
      for (size_t j = 0; j < hash_bucket_map[i].size(); ++j) {
        uint32_t key_index = hash_bucket_map[i][j];
        string key = key_value[key_index].first;
        oss.write(key.c_str(), key.size() + 1);
        oss.write(reinterpret_cast<const char *>(&key_value[key_index].second),
                  sizeof(VT));
        b_size += key.size() + 1 + sizeof(VT);
      }
      hash_buket[i] =
          (static_cast<uint64_t>(pos) << 24) | static_cast<uint64_t>(b_size);
      offset += b_size;
      pos = offset;
    }

    ofstream out("hash_index.bin");
    out.write(oss.str().c_str(), oss.str().size());
    out.write(reinterpret_cast<const char *>(&hash_buket[0]),
              sizeof(hash_buket[0]) * hash_buket.size());
    cout << "hash_buket size: " << buket_size << std::endl;
    out.write(reinterpret_cast<const char *>(&buket_size), sizeof(buket_size));
    return 0;
  }

  template <typename VT> bool readh(const std::string &key, VT &value) {
    uint64_t hash_code = fst::MurmurHash64B(key.c_str(), key.size(), 0);
    std::cout << "hash_code: " << hash_code << std::endl;
    ifstream in("hash_index.bin", ios::binary);
    if (!in) {
      std::cout << "open error" << std::endl;
      return -1;
    }
    in.seekg(-sizeof(uint32_t), ios::end);
    uint32_t bucket_size = 0;
    in.read(reinterpret_cast<char *>(&bucket_size), sizeof(bucket_size));
    std::cout << "bucket_size: " << bucket_size << std::endl;
    size_t index = hash_code % bucket_size;
    std::cout << "index: " << index << std::endl;
    in.seekg(-(sizeof(uint64_t) * bucket_size + sizeof(uint32_t)) +
                 index * sizeof(uint64_t),
             ios::end);
    uint64_t hash_bucket_index = 0;
    in.read(reinterpret_cast<char *>(&hash_bucket_index),
            sizeof(hash_bucket_index));
    if (hash_bucket_index == 0) {
      std::cout << "key not found" << std::endl;
      return false;
    }
    uint64_t pos = hash_bucket_index >> 24;
    uint32_t offset = hash_bucket_index & 0xFFFFFF;
    std::cout << "pos: " << pos << std::endl;
    std::cout << "offset: " << offset << std::endl;
    in.seekg(pos, ios::beg);
    vector<char> buffer(offset);
    std::cout << "buffer size: " << offset << std::endl;
    in.read(buffer.data(), offset);
    string k;
    uint32_t start_index = 0;
    uint32_t k_size = 0;
    uint32_t i = 0;
    for (; i < offset;) {
      if (buffer[i] == '\0') {
        k = string(buffer.data() + start_index, k_size);
        value = *reinterpret_cast<VT *>(&buffer[i + 1]);
        std::cout << k << " : " << value << std::endl;
        if (k == key) {
          std::cout << "key found" << std::endl;
          return true;
        }
        start_index = i + sizeof(VT) + 1;
        k_size = 0;
        i += sizeof(VT) + 1;
      } else {
        k_size += 1;
        ++i;
      }
    }
    std::cout << "i: " << i << std::endl;
    std::cout << "key not found" << std::endl;
    return false;
  }

private:
  // 全局队列 & 同步
  std::queue<compresstask> task_queue;
  std::queue<compressresult> result_queue;
  std::mutex task_mtx;
  std::mutex res_mtx;
  std::condition_variable task_cv;
  std::condition_variable res_cv;
  std::vector<std::pair<std::string, ValueBlockPosIndex>> index_record;

  std::atomic<size_t> task_seq{0}; // 分配任务的全局序号
  std::atomic<bool> read_finished{false};
  std::atomic<bool> compress_finished{false};
};

} // namespace fstd

// ===================== 主控制流 =====================
int main(int argc, char *argv[]) {
  // 模拟待压缩文件列表
  // vector<string> files_to_compress = {"file1.bin", "file2.bin"};
  // string output_archive = "compressed_archive.zst_pack";

  if (argc < 3) {
    cout << "用法: " << argv[0] << "compress/extract 输入文件夹" << endl;
    return 1;
  }

  fstd::FstddCompressor dd_compressor;
  if (string(argv[1]) == "compress") {
    dd_compressor.compress(argv[2], "block.bin");
  } else if (string(argv[1]) == "extract") {
    dd_compressor.extract(argv[2]);
  } else {
    cerr << "compress/extract" << endl;
  }
  return 0;
}