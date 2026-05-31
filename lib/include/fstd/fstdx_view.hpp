#pragma once
#include <set>
#include <unordered_map>
#include <fstd/fstdx_reader.hpp>

using namespace std;
namespace fstd {

class FstdxView {

  //   FstdxView(const
  //   std::unordered_map<std::string,std::string>&fstdx_pathes){

  //   }

  std::unordered_map<std::string, std::vector<std::string>>
  search(std::string_view word, const std::vector<std::string> &names) const {
    std::unordered_map<std::string, std::vector<std::string>> results;
    for (const string &name : names) {
      vector<string> result;
      auto iter = fstdxes.find(name);
      if (iter != fstdxes.end()) {
        bool res = iter->second->exact_match_search(word, result);
        if (res) { results.emplace(name, std::move(result)); }
      }
    }
    return results;
  }

  std::vector<std::string>
  predictive_search(std::string_view word,
                    const std::vector<std::string> &names) const {
    std::set<string> uni_sorted_result;
    for (const string &name : names) {
      auto iter = fstdxes.find(name);
      if (iter != fstdxes.end()) {
        std::vector<std::pair<std::string, uint64_t>> predictive_results =
            iter->second->predictive_search(word);
        if (!predictive_results.empty()) {
          for (auto &p : predictive_results) {
            uni_sorted_result.emplace(std::move(p.first));
          }
        }
      }
    }
    return {uni_sorted_result.cbegin(), uni_sorted_result.cend()};
  }

//   std::vector<std::string>
//   regex_search(std::string_view pattern,
//                const std::vector<std::string> &names) const {
//     return uniq_sort_search(
//         pattern, names,
//         [](std::string_view pattern, const std::shared_ptr<FstdxReader> &ptr) {
//           return ptr->regex_search(pattern);
//         })
//   }

  bool insert(const std::string &name, const std::string &fstdx_path) {
    bool is_valid = false;
    shared_ptr<FstdxReader> ptr =
        make_shared<FstdxReader>(fstdx_path, is_valid);
    if (!is_valid) {
      LOG_ERROR("Insert fstdx failed, as path {} is not a valid fstdx file",
                fstdx_path);
      return false;
    }
    if (fstdxes.find(name) != fstdxes.end()) {
      LOG_ERROR("Insert fstdx failed, as name {} already exists", name);
      return false;
    }
    fstdxes[name] = ptr;
    return true;
  }

private:
  //   std::function<void(size_t, const output_t &)> prefixes = nullptr) const {

  std::vector<std::string>
  uniq_sort_search(std::string_view word, const std::vector<std::string> &names,
                   std::function<std::vector<std::pair<std::string, uint64_t>>(
                       std::string_view, const std::shared_ptr<FstdxReader> &)>
                       search_method) const {

    std::set<string> uni_sorted_result;
    for (const string &name : names) {
      auto iter = fstdxes.find(name);
      if (iter != fstdxes.end()) {
        std::vector<std::pair<std::string, uint64_t>> predictive_results =
            search_method(word, iter->second);
        if (!predictive_results.empty()) {
          for (auto &p : predictive_results) {
            uni_sorted_result.emplace(std::move(p.first));
          }
        }
      }
    }
    return {uni_sorted_result.cbegin(), uni_sorted_result.cend()};
  }

private:
  std::unordered_map<std::string, std::shared_ptr<FstdxReader>> fstdxes;
};

} // namespace fstd