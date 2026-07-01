//
//  Copyright (c) 2026 Moujie Qin. All rights reserved.
//  MIT License
//
#pragma once

#include <fstd/fstdd_compressor.h>
#include <nlohmann/json.hpp>

namespace fstd {
class FstddWriter {
public:
  int compile_fstdd(const std::string &data_path,
                    const std::string &output_file, const nlohmann::json &meta,
                    size_t block_size_kb, size_t compress_level,
                    size_t worker_num, bool opt_verbose);

  int compile_fstdd(const std::vector<std::string> &data_paths,
                    const std::string &output_file, const nlohmann::json &meta,
                    size_t block_size_kb, size_t compress_level,
                    size_t worker_num, bool opt_verbose,
                    size_t file_stream_num = 0);

  int compile_fstdd(const std::vector<std::string> &data_paths,
                    const std::string &output_file,
                    const std::string &meta_json_str, size_t block_size_kb,
                    size_t compress_level, size_t worker_num, bool opt_verbose,
                    size_t file_stream_num);

  bool push_file_stream(const std::string &file_path, std::string_view stream);

private:
  FstddCompressor dd_compressor;
  static const nlohmann::json meta_default;
};
} // namespace fstd