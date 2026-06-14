#pragma once

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
#include <fstd/hash_index.h>
#include <fstd/logger.h>
#include <nlohmann/json.hpp>

namespace fstd {

// ===================== 【核心配置】完全按你的最优方案 =====================
const size_t disk_read_size = 512 * 1024;  // 磁盘读取：512kb（最快）
const size_t zstd_block_size = 128 * 1024; // zstd 压缩块：128kb（最优）
const int zstd_level = 3;        // 压缩级别（3=速度优先，5=压缩率优先）
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

std::ostream &operator<<(std::ostream &os, const ValueBlockPosIndex &vbp_idx);

class FstddCompressor {
public:
  bool compress(const std::string &data_path, std::ofstream &out,
                DdJsonHeader &header);

private:
  // -------------------- 工具：zstd 单块压缩 --------------------
  std::vector<char> zstd_compress_block(const char *data, size_t size,
                                        int level);

  // -------------------- 线程1：单线程顺序读取 & 切块 --------------------
  void read_files(const std::vector<std::string> &data_paths,
                  DdJsonHeader &header);

  // -------------------- 线程池：多线程并行压缩 --------------------
  void compress_worker();

  // -------------------- 线程3：单线程严格顺序写入 --------------------
  void write_output(std::ostream &out, DdJsonHeader &header);

  // bool extract_impl(const string &key, const string &dst_dir);

private:
  // 全局队列 & 同步
  std::queue<CompressTask> task_queue;
  std::queue<CompressResult> result_queue;
  std::mutex task_mtx;
  std::mutex res_mtx;
  std::condition_variable task_cv;
  std::condition_variable res_cv;
  std::vector<std::pair<std::string, ValueBlockPosIndex>> index_record;

  std::atomic<size_t> task_seq{0}; // 分配任务的全局序号
  std::atomic<bool> success{true};
  std::atomic<bool> read_finished{false};
  std::atomic<bool> compress_finished{false};
};

} // namespace fstd
