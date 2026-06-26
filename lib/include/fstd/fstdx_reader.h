#pragma once

#include <fstream>
#include <set>
#include <zstd.h>

#include <fstd/common.h>
#include <fstd/fstlib_wrapper.h>
#include <indicators/block_progress_bar.hpp>
#include <nlohmann/json.hpp>

namespace fstd {

class FstdxHashReader {
public:
  FstdxHashReader(const std::string &fstdx_path);

  ~FstdxHashReader();

  const nlohmann::json &get_meta() const;

  const DxJsonHeader &get_header() const;

  size_t get_key_size() const;

  operator bool() const;

  std::vector<std::string> extract_values() const;

  std::vector<std::string>
  extract_values(DyProgBars<indicators::BlockProgressBar> &dynamic_bars) const;

  bool hash_exact_match_search(std::string_view word,
                               std::vector<std::string> &result) const;

protected:
  bool exact_match_search_by_index_code(uint64_t idx_code,
                                        std::vector<std::string> &result) const;

  bool parse_fstdx(const std::string &fstdx_path);

  std::pair<uint32_t, uint32_t> extract_index(uint64_t index) const;

  bool read_entry_index(std::ifstream &in, const size_t idx,
                        EntryIndex &entry_index) const;

  size_t
  bin_search_block_index(uint32_t entry_index,
                         const std::vector<BlockIndex> &block_indexes) const;

  std::string read_text_by_index(const size_t idx) const;

  std::vector<std::string>
  extract_comp_blocks(std::function<void(size_t)> refresh_bar = nullptr) const;

protected:
  const std::string fstdx_path_;
  bool is_valid_;
  DxJsonHeader mx_json_header_;
  size_t key_size_;
  ZSTD_DDict *ddict_;
  std::vector<BlockIndex> block_indexes_;
  uint64_t comp_text_offset_;
  std::set<size_t> dup_idxes_;
  size_t bucket_size_;
  uint64_t hash_bucket_offset_;
  uint64_t hash_index_offset_;
  uint64_t entry_indexes_offset_;
};

class FstdxReader : public FstdxHashReader {
public:
  FstdxReader(const std::string &fstdx_path);

  size_t get_fst_key_size() const;

  bool exact_match_search(std::string_view word,
                          std::vector<std::string> &result) const;

  std::vector<std::unique_ptr<std::string>>
  common_prefix_search(std::string_view word) const;

  size_t longest_prefix_len(std::string_view word) const;

  std::vector<std::unique_ptr<std::string>>
  predictive_search(std::string_view word) const;

  std::vector<std::unique_ptr<std::string>>
  edit_distance_search(std::string_view word, size_t edit_distance = 1) const;

  std::vector<std::vector<std::unique_ptr<std::string>>> prefix_distance_search(
      std::string_view sv, size_t max_distance, const size_t longest_prefix_len,
      const std::shared_ptr<std::set<std::string>> &prior_suffixes) const;

  std::vector<std::unique_ptr<std::pair<double, std::string>>>
  suggest(std::string_view word) const;

  std::pair<std::vector<std::unique_ptr<std::string>>, std::string>
  regex_search(std::string_view pattern) const;

  std::pair<std::vector<std::unique_ptr<std::string>>, std::string>
  regex_search(std::string_view pattern, ThreadPool &thread_pool) const;

  std::vector<std::tuple<double, std::string, uint64_t>>
  spellcheck_word(std::string_view word, const size_t n = 10) const;

  std::vector<std::pair<std::string, uint64_t>> enumerate() const;

  bool extract(const std::string &output_file);

  std::vector<std::string> extract_keys() const;

private:
  std::vector<std::pair<std::string, uint64_t>>
  enumerate(std::function<void(const size_t)> refresh_bar) const;

  std::vector<std::string>
  extract_keys(DyProgBars<indicators::BlockProgressBar> &dynamic_bars) const;

  std::vector<std::string> extract_keys(
      std::vector<std::pair<std::string, uint64_t>> &&key_output) const;

private:
  FstMapSearcher<uint64_t> fst_map_searcher_;
  size_t fst_key_size_;
};
} // namespace fstd