#pragma once

#include <set>
#include <unordered_map>

#include <fstd/fstdd_reader.h>
#include <fstd/fstdx_reader.h>
#include <nlohmann/json.hpp>

namespace fstd {

class FstdxSearcher {

public:
  FstdxSearcher(size_t worker_num = 0);

  FstdxSearcher(const std::string &meta_json_path, size_t worker_num = 0);

  operator bool() const;

  bool extract(const std::string &name, const std::string &file_path,
               const std::string &dst_dir) const;

  bool extract(const std::string &name, const std::string &file_path) const;

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

private:
  bool load_file(const std::string &meta_json_path);

  std::vector<std::string> find_fstdd(const std::string &target_dir) const;

  std::vector<std::string> uniq_sort_results(
      std::vector<std::vector<std::unique_ptr<std::string>>> &&results,
      size_t count) const;

  std::vector<std::string> uniq_sort_results(
      std::vector<std::vector<std::unique_ptr<std::pair<double, std::string>>>>
          &&results,
      size_t count) const;

  bool has_prior_suffix(const std::string &word) const;

  bool same_prefix_distance_cmp(const std::string &x,
                                const std::string &y) const;

  std::vector<std::string> uniq_sort_results(
      std::vector<std::vector<std::vector<std::unique_ptr<std::string>>>>
          &&results,
      const std::vector<size_t> &counts, size_t count) const;

private:
  bool is_valid_;
  std::shared_ptr<std::set<std::string>> prior_suffixes_;
  std::shared_ptr<std::set<size_t>> prior_suf_lens_;
  std::unordered_map<std::string, std::shared_ptr<FstdxReader>> fstdxes_;
  std::unordered_map<std::string, std::vector<std::shared_ptr<FstddReader>>>
      fstdds_;
  nlohmann::json meta_json_;
  static std::shared_ptr<ThreadPool> thread_pool_ptr;
};

} // namespace fstd
