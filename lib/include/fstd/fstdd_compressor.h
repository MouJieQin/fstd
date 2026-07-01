//
//  Copyright (c) 2026 Moujie Qin. All rights reserved.
//  MIT License
//
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
  bool compress(const std::vector<std::string> &data_paths, std::ofstream &out,
                DdJsonHeader &header, size_t block_size_kb,
                size_t compress_level, size_t worker_num, bool opt_verbose);

  static std::vector<std::pair<std::string, size_t>> recursive_directory(
      const std::vector<std::string> &data_paths, bool opt_verbose = false,
      std::function<void(size_t, const std::string &)> refresh_bar = nullptr);

private:
  std::vector<char> zstd_compress_block(const char *data, size_t size,
                                        int level);

  void read_file_imple(std::istream &ifs, std::string &&key,
                       std::vector<char> &disk_buf, size_t &buf_size,
                       uint64_t &block_index, size_t block_size);

  void read_files(const std::vector<std::string> &data_paths,
                  DdJsonHeader &header, size_t block_size_kb, bool opt_verbose);

  void compress_worker(size_t compress_level);

  void write_output(std::ostream &out, DdJsonHeader &header,
                    size_t compress_level);

private:
  std::queue<CompressTask> task_queue;
  std::queue<CompressResult> result_queue;
  std::mutex task_mtx;
  std::mutex res_mtx;
  std::condition_variable task_cv;
  std::condition_variable res_cv;
  std::vector<std::pair<std::string, ValueBlockPosIndex>> index_record;

  std::atomic<size_t> task_seq{0}; // assign task sequence number
  std::atomic<bool> success{true};
  std::atomic<bool> read_finished{false};
  std::atomic<bool> compress_finished{false};
};

} // namespace fstd
