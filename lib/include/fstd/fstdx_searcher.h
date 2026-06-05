#pragma once

#include <set>
#include <unordered_map>
#include <nlohmann/json.hpp>

#include <fstd/fstdx_compressor.h>
#include <fstd/fstdx_reader.h>

namespace fstd {

class FstdxSearcher {

public:
  FstdxSearcher() = default;
  FstdxSearcher(const std::string &meta_json_path, bool &is_valid);

  std::vector<std::string> search(std::string_view word,
                                  const std::string &name);

  std::unordered_map<std::string, std::vector<std::string>>
  search(std::string_view word, const std::vector<std::string> &names) const;

  std::vector<std::string>
  common_prefix_search(std::string_view word,
                       const std::vector<std::string> &names) const;

  size_t
  longest_common_prefix_search(std::string_view word,
                               const std::vector<std::string> &names) const;

  std::vector<std::string>
  edit_distance_search(std::string_view word,
                       const std::vector<std::string> &names,
                       size_t edit_distance = 1) const;

  std::vector<std::string>
  predictive_search(std::string_view word,
                    const std::vector<std::string> &names) const;

  std::vector<std::string> suggest(std::string_view word,
                                   const std::vector<std::string> &names) const;

  std::vector<std::string>
  prefix_distance_search(std::string_view word,
                         const std::vector<std::string> &names,
                         size_t max_distance) const;

  std::pair<std::vector<std::string>, std::string>
  regex_search(std::string_view pattern,
               const std::vector<std::string> &names) const;

  void insert_if_not_exists(const std::string &name,
                            const std::string &fstdx_path);

  bool insert(const std::string &name, const std::string &fstdx_path);

  bool save_to_disk(const std::string &meta_json_path);

  bool build_fst_index(const std::string &fst_index_path = "");

private:
  bool load_file(const std::string &meta_json_path);

  bool save_fst_index_to_disk(const std::string &fst_index_path);

  bool load_fst_index(const std::string &fst_index_path);

  bool match_index(fst::uint64bit index, uint64_t mask) const;

  std::pair<uint64_t, std::vector<std::string>>
  cost_analysis(const std::vector<std::string> &names) const;

  std::vector<std::string>
  uniq_sort_search(std::string_view word, const std::vector<std::string> &names,
                   std::function<std::vector<std::pair<std::string, uint64_t>>(
                       std::string_view, const std::shared_ptr<FstdxReader> &)>
                       search_method) const;

private:
  FstMapSearcher<fst::uint64bit> fst_indexes_searcher_;
  size_t fst_indexes_size_;
  std::vector<std::string> fst_indexes_names_;
  std::set<std::string> fst_indexes_names_set_;
  std::unordered_map<std::string, std::shared_ptr<FstdxReader>> fstdxes_;
  nlohmann::json meta_json_;
  FstdxCompressor compressor;
};

} // namespace fstd
