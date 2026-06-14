#pragma once

#include <fstream>
#include <nlohmann/json.hpp>

#include <fstd/common.h>
#include <fstd/fstdx_compressor.h>
#include <fstd/fstlib_wrapper.h>

namespace fstd {

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

private:
  const std::string fstdx_path_;
  DxJsonHeader mx_json_header_;
  size_t key_size_;
  size_t fst_key_size_;
  FstMapSearcher<uint64_t> fst_map_searcher_;
  FstdxCompressor dx_compressor_;
  ZSTD_DDict *ddict_;
  std::vector<BlockIndex> block_indexes_;
  std::vector<EntryIndex> entry_indexes_;
  uint64_t comp_text_offset_;
  std::set<size_t> dup_idxes_;
  size_t bucket_size_;
  uint64_t hash_bucket_offset_;
  uint64_t hash_index_offset_;
};
} // namespace fstd