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
      auto iter = fstdxes_.find(name);
      if (iter != fstdxes_.end()) {
        bool res = iter->second->exact_match_search(word, result);
        if (res) { results.emplace(name, std::move(result)); }
      }
    }
    return results;
  }

  std::vector<std::string>
  predictive_search(std::string_view word,
                    const std::vector<std::string> &names) const {
    std::pair<uint64_t, std::vector<std::string>> analysis_result =
        cost_analysis(names);
    uint64_t index = analysis_result.first;
    const std::vector<std::string> &not_indexes_names = analysis_result.second;
    std::vector<std::pair<std::string, fst::uint64bit>> indexes_search_result;
    if (index != 0) {
      indexes_search_result = fst_indexes_searcher_.predictive_search(word);
    }
    std::vector<std::string> filtered_indexes_search_result;
    for (const auto &p : indexes_search_result) {
      if (match_index(p.second, index)) {
        filtered_indexes_search_result.emplace_back(std::move(p.first));
      }
    }
    if (not_indexes_names.empty()) {
      return sort_container(std::move(filtered_indexes_search_result));
    }

    std::unordered_set<std::string> uni_result;
    for (const string &name : names) {
      auto iter = fstdxes_.find(name);
      if (iter == fstdxes_.end()) {
        LOG_ERROR("FstdxView::predictive_search, name {} not found", name);
      } else {
        std::vector<std::pair<std::string, uint64_t>> predictive_results =
            iter->second->predictive_search(word);
        if (!predictive_results.empty()) {
          for (auto &p : predictive_results) {
            uni_result.emplace(std::move(p.first));
          }
        }
      }
    }
    for (const string &p : filtered_indexes_search_result) {
      uni_result.emplace(std::move(p));
    }
    std::vector<std::string> result(uni_result.begin(), uni_result.end());
    return sort_container(std::move(result));
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
    if (fstdxes_.find(name) != fstdxes_.end()) {
      LOG_ERROR("Insert fstdx failed, as name {} already exists", name);
      return false;
    }
    bool is_valid = false;
    shared_ptr<FstdxReader> ptr =
        make_shared<FstdxReader>(fstdx_path, is_valid);
    if (!is_valid) {
      LOG_ERROR("Insert fstdx failed, as path {} is not a valid fstdx file",
                fstdx_path);
      return false;
    }
    fstdxes_[name] = ptr;
    return true;
  }

  bool build_fst_index() {
    if (fstdxes_.size() > 64) {
      LOG_ERROR("Build fst index failed, as fstdx count {} is greater than 64",
                fstdxes_.size());
      return false;
    }
    std::unordered_map<std::string, fst::uint64bit> unordered_input;
    uint64_t index = 1;
    for (const auto &item : fstdxes_) {
      fst_indexes_names_.emplace_back(item.first);
      fst_indexes_names_set_.emplace(item.first);
      std::vector<std::pair<std::string, uint64_t>> key_value =
          item.second->enumerate();
      std::cout << "key_value size: " << key_value.size() << std::endl;
      for (auto &p : key_value) {
        auto iter = unordered_input.find(p.first);
        if (iter == unordered_input.end()) {
          unordered_input.emplace(std::move(p.first), index);
        } else {
          iter->second.bits |= index;
        }
      }
      index <<= 1;
    }
    ostringstream fst_str_stream;
    vector<pair<string, fst::uint64bit>> v_input(unordered_input.begin(),
                                                 unordered_input.end());
    {
      unordered_map<string, fst::uint64bit> tmp;
      unordered_input.swap(tmp);
    }
    vector<size_t> indices(v_input.size());
    iota(indices.begin(), indices.end(), 0);
    sort(indices.begin(), indices.end(), [&](size_t i, size_t j) {
      return v_input[i].first < v_input[j].first;
    });
    vector<pair<string, fst::uint64bit>> sorted_input;
    sorted_input.reserve(v_input.size());
    for (size_t i : indices) {
      sorted_input.emplace_back(std::move(v_input[i].first), v_input[i].second);
    }

    ofstream of("view_index.txt");
    for (const auto &p : sorted_input) {
      of << p.first << ": " << p.second.bits << endl;
    }
    if (!fstd::compile_fst(sorted_input, fst_str_stream, true, true)) {
      return false;
    }
    string fst_str = fst_str_stream.str();
    std::vector<char> key_fst_byte_code(fst_str.begin(), fst_str.end());
    fst_indexes_searcher_ =
        FstMapSearcher<fst::uint64bit>(std::move(key_fst_byte_code));
    fst_indexes_size_ = sorted_input.size();
    std::cout << "FST index built, size: " << sorted_input.size() << std::endl;
    // for (const auto &p :
    // fst_indexes_searcher_.enumerate(sorted_input.size())) {
    //   std::cout << p.first << " : " << p.second.bits << "\n";
    // }
    // std::cout << std::endl;
    return true;
  }

private:
  bool match_index(fst::uint64bit index, uint64_t mask) const {
    return (index.bits & mask) != 0;
  }

  std::pair<uint64_t, std::vector<std::string>>
  cost_analysis(const std::vector<std::string> &names) const {
    if (names.size() < 2) { return {0, names}; }
    size_t key_size = 0;
    for (const string &name : names) {
      auto iter = fstdxes_.find(name);
      if (iter != fstdxes_.end()) {
        key_size += iter->second->get_fst_key_size();
      }
    }
    if (key_size < fst_indexes_size_) { return {0, names}; }
    uint64_t index = 1;
    uint64_t result_index = 0;
    std::vector<std::string> result_names;
    std::unordered_set<std::string> names_set(names.begin(), names.end());
    for (const string &name : fst_indexes_names_) {
      if (names_set.find(name) != names_set.end()) {
        result_index |= index;
      } else {
        result_names.emplace_back(name);
      }
      index <<= 1;
    }
    return {result_index, result_names};
  }

  std::vector<std::string>
  uniq_sort_search(std::string_view word, const std::vector<std::string> &names,
                   std::function<std::vector<std::pair<std::string, uint64_t>>(
                       std::string_view, const std::shared_ptr<FstdxReader> &)>
                       search_method) const {

    std::set<string> uni_sorted_result;
    for (const string &name : names) {
      auto iter = fstdxes_.find(name);
      if (iter != fstdxes_.end()) {
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
  FstMapSearcher<fst::uint64bit> fst_indexes_searcher_;
  size_t fst_indexes_size_;
  std::vector<std::string> fst_indexes_names_;
  std::set<std::string> fst_indexes_names_set_;
  std::unordered_map<std::string, std::shared_ptr<FstdxReader>> fstdxes_;
};

} // namespace fstd

int main(int argc, char *argv[]) {
  fstd::FstdxView view;
  for (int i = 1; i < argc; ++i) {
    view.insert(std::to_string(i), argv[i]);
  }
  view.build_fst_index();
  while (true) {
    std::string word;
    std::string cmd;
    std::cout << "Enter command (search/predict/exit): " << std::endl;
    std::cin >> cmd;

    if (cmd == "search") {
      std::cin >> word;
      std::string name;
      std::vector<std::string> names;
      while (std::cin >> name) {
        if (name == "end") { break; }
        names.emplace_back(name);
      }
      std::unordered_map<std::string, std::vector<std::string>> results =
          view.search(word, names);
      for (const auto &p : results) {
        std::cout << "name: " << p.first << "\n";
        for (const auto &item : p.second) {
          std::cout << item << "\n";
        }
      }
    } else if (cmd == "predict") {
      std::cin >> word;
      std::string name;
      std::vector<std::string> names;
      while (std::cin >> name) {
        if (name == "end") { break; }
        names.emplace_back(name);
      }
      std::vector<std::string> results = view.predictive_search(word, names);
      std::cout << "predictive search result:\n";
      for (const auto &item : results) {
        std::cout << item << "\n";
      }
    } else if (cmd == "exit") {
      break;
    } else {
      LOG_ERROR("Unknown command: {}", word);
    }
    if (cmd == "exit") { break; }
  }
}