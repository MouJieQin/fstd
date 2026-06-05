#pragma once

#include <fstream>
#include <nlohmann/json.hpp>

#include <fstd/fstdx_compressor.h>
#include <fstd/fstlib_wrapper.h>

namespace fstd {
#define FSTD_VERSION "0.1.0"

struct MxHeaderSizeRecord {
  MxHeaderSizeRecord() = default;
  MxHeaderSizeRecord(uint32_t original_size, uint32_t compressed_size);
  uint32_t original_size;
  uint32_t compressed_size;
};

using MxJsonHeader = nlohmann::json;

class FstdxReader {

public:
  FstdxReader(const std::string &fstdx_path, bool &is_valid);

  ~FstdxReader();

  const nlohmann::json &get_meta() const;

  size_t get_key_size() const;

  size_t get_fst_key_size() const;

  std::pair<uint32_t, uint32_t> extract_index(uint64_t index) const;

  bool exact_match_search(std::string_view word,
                          std::vector<std::string> &result) const;

  std::vector<std::pair<std::string, uint64_t>>
  common_prefix_search(std::string_view word) const;

  size_t
  longest_common_prefix_search(std::string_view word,
                               std::pair<std::string, uint64_t> &result) const;

  std::vector<std::pair<std::string, uint64_t>>
  predictive_search(std::string_view word) const;

  std::vector<std::pair<std::string, uint64_t>>
  edit_distance_search(std::string_view word, size_t edit_distance = 1) const;

  std::pair<std::vector<std::pair<std::string, uint64_t>>, std::string>
  regex_search(std::string_view pattern) const;

  std::vector<std::tuple<double, std::string, uint64_t>>
  spellcheck_word(std::string_view word, const size_t n = 10) const;

  std::vector<std::pair<std::string, uint64_t>> enumerate() const;

  std::vector<std::string> extract_values() const;

  std::vector<std::string> extract_keys() const;

private:
  bool parse_fstdx(const std::string &fstdx_path);

  template <typename T>
  bool decompress(std::istream &ins, const std::string &block_name,
                  const MxJsonHeader &mx_json_header_, std::vector<T> &con) {
    const nlohmann::json &json_block = mx_json_header_[block_name];
    int compress_level = json_block["compress_level"];
    uint64_t offset = json_block["offset"];
    uint64_t original_size = json_block["original_size"];
    std::vector<T> tmp_con;
    tmp_con.resize(original_size / sizeof(T));
    if (compress_level == 0) {
      ins.seekg(offset);
      ins.read(reinterpret_cast<char *>(tmp_con.data()), tmp_con.size());
    } else {
      uint64_t compressed_size = json_block["compressed_size"];
      std::vector<char> compressed_block(compressed_size);
      ins.seekg(offset);
      ins.read(compressed_block.data(), compressed_block.size());
      std::vector<char> dst_buff(original_size);
      bool res = dx_compressor_.decompressToBuffer(compressed_block.data(),
                                                   compressed_block.size(),
                                                   original_size, dst_buff);
      if (!res) { return false; }
      memcpy(tmp_con.data(), dst_buff.data(), dst_buff.size());
    }
    con.swap(tmp_con);
    return true;
  }

private:
  const std::string fstdx_path_;
  MxJsonHeader mx_json_header_;
  size_t key_size_;
  size_t fst_key_size_;
  FstMapSearcher<uint64_t> fst_map_searcher_;
  FstdxCompressor dx_compressor_;
  ZSTD_DDict *ddict_;
  std::vector<BlockIndex> block_indexes_;
  std::vector<EntryIndex> entry_indexes_;
  uint64_t comp_text_offset_;
};
} // namespace fstd