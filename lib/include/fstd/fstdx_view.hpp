#pragma once
#include <fstd/fstdx_reader.hpp>
#include <set>
#include <unordered_map>

using namespace std;
namespace fstd {

class FstdxView {
public:
  FstdxView() = default;

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
  //         [](std::string_view pattern, const std::shared_ptr<FstdxReader>
  //         &ptr) {
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

  bool build_fst_index() {
    if (fstdxes.size() > 32) { return false; }
    std::map<std::string, fst::uint32bit> input;
    uint32_t index = 1;
    for (const auto &item : fstdxes) {
      std::vector<std::pair<std::string, uint64_t>> key_value =
          item.second->enumerate();
      std::cout << "key_value size: " << key_value.size() << std::endl;
      for (auto &p : key_value) {
        auto iter = input.find(p.first);
        if (iter == input.end()) {
          input.emplace(std::move(p.first), index);
        } else {
          iter->second.bits |= index;
        }
      }
      index <<= 1;
    }
    ostringstream fst_str_stream;
    vector<pair<string, fst::uint32bit>> v_input(input.begin(), input.end());
    ofstream of("view_index.txt");
    for (const auto &p : v_input) {
      of << p.first << ": " << p.second.bits << endl;
    }
    if (!fstd::compile_fst(v_input, fst_str_stream, true, true)) {
      return false;
    }
    string fst_str = fst_str_stream.str();
    std::vector<char> key_fst_byte_code(fst_str.begin(), fst_str.end());
    fst_indexes_searcher = FstMapSearcher<fst::uint32bit>(std::move(key_fst_byte_code));
    for (const auto &p : fst_indexes_searcher.enumerate(v_input.size())) {
      std::cout << p.first << " : " << p.second.bits << "\n";
    }
    std::cout << std::endl;
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
  FstMapSearcher<fst::uint32bit> fst_indexes_searcher;
  std::unordered_map<std::string, std::shared_ptr<FstdxReader>> fstdxes;
};

} // namespace fstd

int main(int argc, char *argv[]) {
  fstd::FstdxView view;
  for (int i = 1; i < argc; ++i) {
    view.insert(std::to_string(i), argv[i]);
  }
  // view.insert("1", "test.fstdx");
  // view.insert("1", "lj.fstdx");
  // view.insert("2", "lj.fstdx");
  // view.insert("3", "test.fstdx");
  view.build_fst_index();
}