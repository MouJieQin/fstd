#pragma once
#include <fstd/fstdx_reader.hpp>
#include <nlohmann/json.hpp>
#include <sstream>

namespace fstd {
constexpr auto DELIMITER = "</>";

std::string get_current_date();

class FstdxWriter {
public:
  FstdxWriter() = default;

  int compile_fstdx(const std::string &output_file,
                    std::vector<std::string> &keys,
                    std::vector<std::string> &values,
                    const nlohmann::json &meta, uint16_t block_size_kb,
                    uint8_t compress_level, uint16_t zstd_dict_size_kb,
                    bool opt_sorted, bool opt_verbose);

  int compile_fstdx(const std::string &input_file,
                    const std::string &output_file, const nlohmann::json &meta,
                    uint16_t block_size_kb, uint8_t compress_level,
                    uint16_t zstd_dict_size_kb, bool opt_sorted,
                    bool opt_verbose);

  int compile_fstdx(const std::string &output_file,
                    std::vector<std::string> &keys,
                    std::vector<std::string> &values,
                    const std::string &meta_json_str, uint16_t block_size_kb,
                    uint8_t compress_level, uint16_t zstd_dict_size_kb,
                    bool opt_sorted, bool opt_verbose);

  int compile_fstdx(std::ostream &fout, std::vector<std::string> &keys,
                    std::vector<std::string> &values,
                    const nlohmann::json &meta, uint16_t block_size_kb,
                    uint8_t compress_level, uint16_t zstd_dict_size_kb,
                    bool opt_sorted, bool opt_verbose);

  bool extract_fstdx(const std::string &input_file,
                     const std::string &output_file);

private:
  std::string trim_whitespace(const std::string &s);

  bool handle_meta(const nlohmann::json &meta, MxJsonHeader &header);

  bool parse_raw_txt(std::vector<std::string> &raw_lines,
                     std::vector<size_t> &delimiter_indices,
                     std::vector<std::string> &keys,
                     std::vector<std::string> &values);

  bool load_file(ifstream &fin, std::vector<std::string> &keys,
                 std::vector<std::string> &values);

  void sort_keys_values(std::vector<std::string> &keys,
                        std::vector<std::string> &values);

  uint64_t get_output(uint32_t index, uint32_t duplicate_count);

  void make_output(std::vector<std::string> &sorted_keys,
                   std::vector<std::pair<std::string, uint64_t>> &input);

  bool compile_fst(std::vector<std::pair<std::string, uint64_t>> &input,
                   std::ostringstream &oss_out, bool opt_sorted,
                   bool opt_verbose);

private:
  static const nlohmann::json meta_default;
};
} // namespace fstd