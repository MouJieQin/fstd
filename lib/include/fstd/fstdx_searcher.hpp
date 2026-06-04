#pragma once
#include <filesystem>
#include <set>
#include <unordered_map>

#include <fstd/compress_dx.hpp>
#include <fstd/fstdx_reader.hpp>
#include <nlohmann/json.hpp>

using namespace std;
namespace fs = std::filesystem;

namespace fstd {

class FstdxSearcher {
  using json = nlohmann::json;

public:
  FstdxSearcher() = default;
  FstdxSearcher(const std::string &meta_json_path, bool &is_valid) {
    if (!load_file(meta_json_path)) {
      is_valid = false;
      return;
    }
    is_valid = true;
  }

  std::vector<std::string> search(std::string_view word,
                                  const std::string &name) {
    std::vector<std::string> result;
    auto iter = fstdxes_.find(name);
    if (iter != fstdxes_.end()) {
      iter->second->exact_match_search(word, result);
    }
    return result;
  }

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
  common_prefix_search(std::string_view word,
                       const std::vector<std::string> &names) const {
    std::vector<std::pair<std::string, fst::uint64bit>> indexes_search_result;
    auto [index, not_indexes_names] = cost_analysis(names);
    if (index != 0) {
      indexes_search_result = fst_indexes_searcher_.common_prefix_search(word);
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
    return sort_container(std::move(filtered_indexes_search_result));
  }

  size_t
  longest_common_prefix_search(std::string_view word,
                               const std::vector<std::string> &names) const {
    std::pair<std::string, fst::uint64bit> index_result;
    auto [index, not_indexes_names] = cost_analysis(names);
    size_t longest_prefix_len = 0;
    if (index != 0) {
      longest_prefix_len = fst_indexes_searcher_.longest_common_prefix_search(
          word, index_result);
    }
    if (not_indexes_names.empty()) { return longest_prefix_len; }

    std::pair<std::string, uint64_t> result;
    for (const string &name : names) {
      auto iter = fstdxes_.find(name);
      if (iter == fstdxes_.end()) {
        LOG_ERROR(
            "FstdxSearcher::longest_common_prefix_search, name {} not found",
            name);
      } else {
        size_t len = iter->second->longest_common_prefix_search(word, result);
        if (len > longest_prefix_len) { longest_prefix_len = len; }
      }
    }
    return longest_prefix_len;
  }

  std::vector<std::string>
  edit_distance_search(std::string_view word,
                       const std::vector<std::string> &names,
                       size_t edit_distance = 1) const {
    auto [index, not_indexes_names] = cost_analysis(names);
    std::vector<std::pair<std::string, fst::uint64bit>> indexes_search_result;
    if (index != 0) {
      indexes_search_result =
          fst_indexes_searcher_.edit_distance_search(word, edit_distance);
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
        LOG_ERROR("FstdxSearcher::edit_distance_search, name {} not found",
                  name);
      } else {
        std::vector<std::pair<std::string, uint64_t>> edit_distance_results =
            iter->second->edit_distance_search(word, edit_distance);
        if (!edit_distance_results.empty()) {
          for (auto &p : edit_distance_results) {
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

  std::vector<std::string>
  predictive_search(std::string_view word,
                    const std::vector<std::string> &names) const {
    auto [index, not_indexes_names] = cost_analysis(names);

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
        LOG_ERROR("FstdxSearcher::predictive_search, name {} not found", name);
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

  std::vector<std::string>
  suggest(std::string_view word, const std::vector<std::string> &names) const {
    auto [index, not_indexes_names] = cost_analysis(names);
    std::vector<std::tuple<double, std::string, fst::uint64bit>>
        indexes_search_result;
    if (index != 0) {
      indexes_search_result = fst_indexes_searcher_.suggest(word);
    }
    std::vector<std::string> filtered_indexes_search_result;
    for (const auto &p : indexes_search_result) {
      if (match_index(std::get<2>(p), index)) {
        filtered_indexes_search_result.emplace_back(std::get<1>(p));
      }
    }
    if (not_indexes_names.empty()) { return filtered_indexes_search_result; }
    return filtered_indexes_search_result;
  }

  std::vector<std::string>
  prefix_distance_search(std::string_view word,
                         const std::vector<std::string> &names,
                         size_t max_distance) const {
    auto [index, not_indexes_names] = cost_analysis(names);
    std::vector<std::vector<std::pair<std::string, fst::uint64bit>>>
        indexes_search_result;
    if (index != 0) {
      indexes_search_result =
          fst_indexes_searcher_.prefix_distance_search(word, max_distance);
    }
    std::vector<std::string> filtered_indexes_search_result;
    size_t dist = 0;
    for (auto &v : indexes_search_result) {
      std::cout << "v.size(): " << v.size() << " indexes_search_result.size(): "
                << indexes_search_result.size() << std::endl;
      std::vector<size_t> indices;
      indices.reserve(v.size());
      for (size_t i = 0; i < v.size(); ++i) {
        if (match_index(v[i].second, index)) { indices.push_back(i); }
      }
      std::sort(indices.begin(), indices.end(),
                [&](size_t i, size_t j) { return v[i].first < v[j].first; });
      for (size_t i : indices) {
        std::cout << "v[i].first: " << v[i].first << " dist: " << dist
                  << std::endl;
        filtered_indexes_search_result.emplace_back(std::move(v[i].first));
      }
      dist += 1;
    }
    if (not_indexes_names.empty()) { return filtered_indexes_search_result; }
    return filtered_indexes_search_result;
  }

  std::pair<std::vector<std::string>, std::string>
  regex_search(std::string_view pattern,
               const std::vector<std::string> &names) const {
    auto [index, not_indexes_names] = cost_analysis(names);
    std::pair<std::vector<std::pair<std::string, fst::uint64bit>>, std::string>
        indexes_search_result;
    if (index != 0) {
      indexes_search_result = fst_indexes_searcher_.regex_search(pattern);
    }
    if (!indexes_search_result.second.empty()) {
      return {std::vector<std::string>(), indexes_search_result.second};
    }
    std::vector<std::string> filtered_indexes_search_result;
    for (const auto &p : indexes_search_result.first) {
      if (match_index(p.second, index)) {
        filtered_indexes_search_result.emplace_back(std::move(p.first));
      }
    }
    if (not_indexes_names.empty()) {
      return {sort_container(std::move(filtered_indexes_search_result)), ""};
    }
    return {sort_container(std::move(filtered_indexes_search_result)), ""};
  }

  void insert_if_not_exists(const std::string &name,
                            const std::string &fstdx_path) {
    if (fstdxes_.find(name) != fstdxes_.end()) { return; }
    insert(name, fstdx_path);
  }

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
    meta_json_["fstdx"][name] = fs::absolute(fstdx_path).string();
    return true;
  }

  bool save_to_disk(const std::string &meta_json_path) {
    ofstream ofs(meta_json_path, ios::out);
    if (!ofs) {
      LOG_ERROR("Cannot open the file: {}", meta_json_path);
      return false;
    }
    ofs << meta_json_.dump(2);
    if (!ofs) {
      LOG_ERROR("Save meta json file {} failed", meta_json_path);
      return false;
    }
    return true;
  }

  bool build_fst_index(const std::string &fst_index_path = "") {
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
    meta_json_["fst_index"]["names"] = fst_indexes_names_;
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
    meta_json_["fst_index"]["size"] = fst_indexes_size_;
    std::cout << "FST index built, size: " << sorted_input.size() << std::endl;

    if (!fst_index_path.empty()) {
      return save_fst_index_to_disk(fst_index_path);
    }
    return true;
  }

private:
  bool load_file(const std::string &meta_json_path) {
    ifstream ifs(meta_json_path);
    if (!ifs) {
      LOG_ERROR("Failed to open file {} for reading.", meta_json_path);
      return false;
    }
    json meta_json;
    try {
      ifs >> meta_json;
    } catch (const json::exception &e) {
      LOG_ERROR("JSON元数据文件 {} 格式错误：{}", meta_json_path, e.what());
      return false;
    } catch (const std::exception &e) {
      LOG_ERROR("JSON元数据文件 {} 读取错误：{}", meta_json_path, e.what());
      return false;
    }
    meta_json_ = std::move(meta_json);
    if (!meta_json_.contains("fstdx") || !meta_json_["fstdx"].is_object()) {
      LOG_ERROR("Meta JSON does not contain a valid 'fstdx' object.");
      return false;
    }
    const json &fstdx_obj = meta_json_["fstdx"];
    for (auto it = fstdx_obj.begin(); it != fstdx_obj.end(); ++it) {
      const string &name = it.key();
      const string &path = it.value();
      if (!insert(name, path)) {
        LOG_ERROR("Failed to insert fstdx with name {} and path {}", name,
                  path);
        return false;
      }
    }

    try {
      const std::string fst_index_path =
          meta_json_["fst_index"]["path"].get<std::string>();
      if (!load_fst_index(fst_index_path)) {
        LOG_ERROR("Failed to load FST index file {}", fst_index_path);
        return false;
      }
      fst_indexes_size_ = meta_json_["fst_index"]["size"].get<size_t>();
      fst_indexes_names_ =
          meta_json_["fst_index"]["names"].get<vector<string>>();
      fst_indexes_names_set_ = std::set<std::string>(fst_indexes_names_.begin(),
                                                     fst_indexes_names_.end());
    } catch (const json::exception &e) {
      LOG_ERROR("Meta JSON format error: {}", e.what());
      return false;
    } catch (const std::exception &e) {
      LOG_ERROR("Meta JSON processing error: {}", e.what());
      return false;
    }
    return true;
  }

  bool save_fst_index_to_disk(const std::string &fst_index_path) {
    std::ofstream ofs(fst_index_path, std::ios::binary);
    if (!ofs) {
      LOG_ERROR("Cannot open the file: {}", fst_index_path);
      return false;
    }
    auto ptr = fst_indexes_searcher_.get_fst_byte_code();
    meta_json_["fst_index"]["byte_size"] = ptr->size();
    std::vector<char> dst;
    if (!compressor.compressToBuffer(ptr->data(), ptr->size(), dst, 5)) {
      return false;
    }
    ofs.write(dst.data(), dst.size());
    meta_json_["fst_index"]["path"] = fs::absolute(fst_index_path).string();
    return true;
  }

  bool load_fst_index(const std::string &fst_index_path) {
    if (!fs::exists(fst_index_path)) {
      LOG_ERROR("FST index file {} does not exist", fst_index_path);
      return false;
    }
    std::ifstream ifs(fst_index_path, std::ios::binary | std::ios::ate);
    if (!ifs) {
      LOG_ERROR("Cannot open the file: {}", fst_index_path);
      return false;
    }
    size_t file_size = ifs.tellg();
    std::vector<char> src(file_size);
    ifs.seekg(0);
    ifs.read(src.data(), file_size);
    if (!ifs) {
      LOG_ERROR("Read FST index file {} failed", fst_index_path);
      return false;
    }
    size_t original_size = meta_json_["fst_index"]["byte_size"].get<size_t>();
    std::vector<char> dst;
    if (!compressor.decompressToBuffer(src.data(), src.size(), original_size,
                                       dst)) {
      return false;
    }
    fst_indexes_searcher_ = FstMapSearcher<fst::uint64bit>(std::move(dst));
    return true;
  }

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
  json meta_json_;
  Dxcompressor compressor;
};

} // namespace fstd

int main(int argc, char *argv[]) {
  if (argc < 2) {
    LOG_ERROR("Usage: {} <meta_json_path>", argv[0]);
    return 1;
  }
  std::cout << "argv[1]: " << argv[1] << std::endl;
  if (string(argv[1]) == "load") {
    bool is_valid = false;
    fstd::FstdxSearcher view("searcher_meta.json", is_valid);
    if (!is_valid) {
      LOG_ERROR("Failed to load FstdxSearcher from meta JSON file");
      return 1;
    }
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
  } else {
    fstd::FstdxSearcher view;
    for (int i = 1; i < argc; ++i) {
      view.insert(std::to_string(i), argv[i]);
    }
    view.build_fst_index("searcher_fst_index.fstdxidx");
    view.save_to_disk("searcher_meta.json");
    return 0;
  }
}