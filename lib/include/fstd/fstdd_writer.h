#pragma once

#include <nlohmann/json.hpp>
#include <fstd/fstdd_compressor.h>

namespace fstd {
class FstddWriter {
public:
  int compile_fstdd(const std::string &data_path,
                    const std::string &output_file, const nlohmann::json &meta,
                    uint16_t block_size_kb, uint8_t compress_level,
                    size_t worker_num, bool opt_verbose);

private:
  static const nlohmann::json meta_default;
};
} // namespace fstd