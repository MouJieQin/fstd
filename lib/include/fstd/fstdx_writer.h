#pragma once
#include <fstd/common.h>
#include <sstream>

namespace fstd {

class FstdxWriter {
public:
  FstdxWriter() = default;

  int compile_fstdx(const std::string &output_file,
                    std::vector<std::string> &keys,
                    std::vector<std::string> &values,
                    const nlohmann::json &meta, uint16_t block_size_kb,
                    uint8_t compress_level, uint16_t zstd_dict_size_kb,
                    size_t worker_num, bool opt_sorted, bool opt_verbose);

  int compile_fstdx(const std::string &input_file,
                    const std::string &output_file, const nlohmann::json &meta,
                    uint16_t block_size_kb, uint8_t compress_level,
                    uint16_t zstd_dict_size_kb, size_t worker_num,
                    bool opt_sorted, bool opt_verbose);

  int compile_fstdx(const std::string &output_file,
                    std::vector<std::string> &keys,
                    std::vector<std::string> &values,
                    const std::string &meta_json_str, uint16_t block_size_kb,
                    uint8_t compress_level, uint16_t zstd_dict_size_kb,
                    size_t worker_num, bool opt_sorted, bool opt_verbose);

  int compile_fstdx(std::ostream &fout, std::vector<std::string> &keys,
                    std::vector<std::string> &values,
                    const nlohmann::json &meta, uint16_t block_size_kb,
                    uint8_t compress_level, uint16_t zstd_dict_size_kb,
                    size_t worker_num, bool opt_sorted, bool opt_verbose);

  bool extract_fstdx(const std::string &input_file,
                     const std::string &output_file);

  bool load_file(const std::string &file_path, std::vector<std::string> &keys,
                 std::vector<std::string> &values);

private:
  std::string trim_whitespace(const std::string &s);

  bool parse_raw_txt(std::vector<std::unique_ptr<std::string>> &raw_lines,
                     std::vector<size_t> &delimiter_indices,
                     std::vector<std::string> &keys,
                     std::vector<std::string> &values);

  bool load_file(std::ifstream &fin, std::vector<std::string> &keys,
                 std::vector<std::string> &values);

  void sort_keys_values(std::vector<std::string> &keys,
                        std::vector<std::string> &values);

  uint64_t get_output(uint32_t index, uint32_t duplicate_count);

  void make_output(std::vector<std::string> &&sorted_keys,
                   std::vector<std::pair<std::string, uint64_t>> &input);

  bool compile_fst(std::vector<std::pair<std::string, uint64_t>> &input,
                   std::ostringstream &oss_out, bool opt_sorted,
                   bool opt_verbose, std::function<void(size_t)> progress);

  int compile_fstdx_impl(std::ostream &fout,
                         std::vector<std::pair<std::string, uint64_t>> &input,
                         std::vector<std::string> &values, DxJsonHeader &header,
                         uint16_t block_size_kb, uint8_t compress_level,
                         uint16_t zstd_dict_size_kb, size_t worker_num,
                         bool opt_verbose);

  int write_fst_header(std::ostream &fout, std::ostringstream &oss_key_fst_out,
                       DxJsonHeader &header, uint8_t compress_level);

private:
  static const nlohmann::json meta_default;
};
} // namespace fstd