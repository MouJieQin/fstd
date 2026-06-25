#include <filesystem>
#include <unordered_set>

#include <fstd/fstdx_searcher.h>
#include <fstd/logger.h>

using namespace std;
namespace fs = std::filesystem;
using json = nlohmann::json;

namespace fstd {

std::shared_ptr<ThreadPool> FstdxSearcher::thread_pool_ptr = nullptr;

FstdxSearcher::FstdxSearcher(size_t worker_num) : is_valid_(true) {
  thread_pool_ptr = make_shared<ThreadPool>(worker_num);
}

FstdxSearcher::FstdxSearcher(const std::string &meta_json_path,
                             size_t worker_num) {
  if (!load_file(meta_json_path)) {
    is_valid_ = false;
    return;
  }
  thread_pool_ptr = make_shared<ThreadPool>(worker_num);
  is_valid_ = true;
}

FstdxSearcher::operator bool() const { return is_valid_; }

bool FstdxSearcher::extract(const std::string &name,
                            const std::string &file_path,
                            const std::string &dst_dir) const {
  auto iter = fstdds_.find(name);
  if (iter == fstdds_.end()) { return false; }
  for (auto &ptr : iter->second) {
    if (ptr->extract(file_path, dst_dir)) { return true; }
  }
  return false;
}

bool FstdxSearcher::extract(const std::string &name,
                            const std::string &file_path) const {
  const json &fstdx_obj = meta_json_["fstdx"];
  auto iter = fstdx_obj.find(name);
  if (iter == fstdx_obj.end()) { return false; }

  fs::path default_dst_dir = fs::path(*iter).parent_path() / "data";
  return extract(name, file_path, default_dst_dir.string());
}

std::vector<std::string> FstdxSearcher::search(std::string_view word,
                                               const std::string &name) {
  std::vector<std::string> result;
  auto iter = fstdxes_.find(name);
  if (iter != fstdxes_.end()) {
    iter->second->hash_exact_match_search(word, result);
  }
  return result;
}

std::unordered_map<std::string, std::vector<std::string>>
FstdxSearcher::search(std::string_view word,
                      const std::vector<std::string> &names) const {
  std::unordered_map<std::string, std::vector<std::string>> results;
  for (const string &name : names) {
    vector<string> result;
    auto iter = fstdxes_.find(name);
    if (iter != fstdxes_.end()) {
      bool res = iter->second->hash_exact_match_search(word, result);
      if (res) { results.emplace(name, std::move(result)); }
    }
  }
  return results;
}

std::vector<std::string> FstdxSearcher::common_prefix_search(
    std::string_view word, const std::vector<std::string> &names) const {
  vector<vector<unique_ptr<string>>> results;
  results.reserve(names.size());
  size_t count = 0;
  for (const string &name : names) {
    auto iter = fstdxes_.find(name);
    if (iter == fstdxes_.end()) {
      LOG_ERROR("FstdxSearcher: name [{}] not found", name);
    } else {
      vector<unique_ptr<string>> res = iter->second->common_prefix_search(word);
      count += res.size();
      results.emplace_back(std::move(res));
    }
  }
  return uniq_sort_results(std::move(results), count);
}

size_t FstdxSearcher::longest_common_prefix_search(
    std::string_view word, const std::vector<std::string> &names) const {
  size_t longest_prefix_len = 0;
  for (const string &name : names) {
    auto iter = fstdxes_.find(name);
    if (iter == fstdxes_.end()) {
      LOG_ERROR("FstdxSearcher: name {} not found", name);
    } else {
      size_t len = iter->second->longest_common_prefix_search(word);
      if (len > longest_prefix_len) { longest_prefix_len = len; }
    }
  }
  return longest_prefix_len;
}

std::vector<std::string>
FstdxSearcher::edit_distance_search(std::string_view word,
                                    const std::vector<std::string> &names,
                                    size_t edit_distance) const {

  vector<vector<unique_ptr<string>>> results;
  results.reserve(names.size());
  size_t count = 0;
  for (const string &name : names) {
    auto iter = fstdxes_.find(name);
    if (iter == fstdxes_.end()) {
      LOG_ERROR("FstdxSearcher::edit_distance_search, name [{}] not found",
                name);
    } else {
      vector<unique_ptr<string>> res =
          iter->second->edit_distance_search(word, edit_distance);
      count += res.size();
      results.emplace_back(std::move(res));
    }
  }
  return uniq_sort_results(std::move(results), count);
}

// std::vector<std::string> FstdxSearcher::search_impl(
//     std::string_view word, const std::vector<std::string> &names,
//     std::function<std::vector<std::unique_ptr<std::string>>(
//         std::shared_ptr<FstdxReader> &, std::string_view)>
//         search_func) const {
//   vector<string> result;
//   vector<vector<unique_ptr<string>>> results;
//   results.reserve(names.size());
//   size_t count = 0;
//   for (const string &name : names) {
//     auto iter = fstdxes_.find(name);
//     if (iter == fstdxes_.end()) {
//       LOG_ERROR("FstdxSearcher:: name [{}] not found", name);
//     } else {
//       vector<unique_ptr<string>> res = search_func(iter->second, word);
//       iter->second->predictive_search(word);
//       count += res.first.size();
//       results.emplace_back(std::move(res.first));
//     }
//   }
//   return {uniq_sort_results(std::move(results), count), ""};
// }

std::vector<std::string>
FstdxSearcher::predictive_search(std::string_view word,
                                 const std::vector<std::string> &names) const {
  vector<vector<unique_ptr<string>>> results;
  results.reserve(names.size());
  size_t count = 0;
  for (const string &name : names) {
    auto iter = fstdxes_.find(name);
    if (iter == fstdxes_.end()) {
      LOG_ERROR("FstdxSearcher: name [{}] not found", name);
    } else {
      vector<unique_ptr<string>> res = iter->second->predictive_search(word);
      count += res.size();
      results.emplace_back(std::move(res));
    }
  }
  return uniq_sort_results(std::move(results), count);
}

std::vector<std::string> FstdxSearcher::uniq_sort_results(
    std::vector<std::vector<std::unique_ptr<std::pair<double, std::string>>>>
        &&results,
    size_t count) const {
  vector<string> result;
  if (count == 0) { return result; }
  vector<unique_ptr<pair<double, string>>> result_ptrs;
  result.reserve(count);
  result_ptrs.reserve(count);
  for (auto &res : results) {
    for (auto &ptr : res) {
      result_ptrs.emplace_back(std::move(ptr));
    }
  }

  std::sort(result_ptrs.begin(), result_ptrs.end(), [&](auto &x, auto &y) {
    return x->first == y->first ? x->second < y->second : x->first > y->first;
  });

  // unique
  size_t index = 0;
  for (size_t i = 1; i < result_ptrs.size(); i++) {
    auto &x = result_ptrs[index]->second;
    auto &y = result_ptrs[i]->second;
    if (!(x.size() == y.size() && x == y)) {
      result.emplace_back(std::move(x));
      index = i;
    }
  }
  // handle the last key
  result.emplace_back(std::move(result_ptrs[index]->second));
  return result;
}

std::vector<std::string>
FstdxSearcher::suggest(std::string_view word,
                       const std::vector<std::string> &names) const {
  vector<vector<unique_ptr<pair<double, string>>>> results;
  results.reserve(names.size());
  size_t count = 0;
  for (const string &name : names) {
    auto iter = fstdxes_.find(name);
    if (iter == fstdxes_.end()) {
      LOG_ERROR("FstdxSearcher: name [{}] not found", name);
    } else {
      vector<unique_ptr<pair<double, string>>> res =
          iter->second->suggest(word);
      count += res.size();
      results.emplace_back(std::move(res));
    }
  }
  return uniq_sort_results(std::move(results), count);
}

std::vector<std::string> FstdxSearcher::uniq_sort_results(
    std::vector<std::vector<std::vector<std::unique_ptr<std::string>>>>
        &&results,
    std::vector<size_t> counts, size_t count) const {
  vector<string> result;
  if (count == 0) { return result; }
  size_t distance = results[0].size();
  vector<vector<unique_ptr<string>>> result_ptrs(distance);
  result.reserve(count);
  for (auto &result : results) {
    for (size_t i = 0; i < distance; i++) {
      result_ptrs[i].reserve(counts[i]);
      for (auto &ptr : result[i]) {
        result_ptrs[i].emplace_back(std::move(ptr));
      }
      std::sort(result_ptrs[i].begin(), result_ptrs[i].end(),
                [&](auto &x, auto &y) { return *x < *y; });
    }
  }
  for (auto &res_ptr : result_ptrs) {
    for (auto &ptr : res_ptr) {
      result.emplace_back(std::move(*ptr));
    }
  }
  return result;
}

std::vector<std::string>
FstdxSearcher::prefix_distance_search(std::string_view word,
                                      const std::vector<std::string> &names,
                                      size_t max_distance) const {
  vector<vector<vector<unique_ptr<string>>>> results;
  results.reserve(names.size());
  size_t count = 0;
  vector<size_t> counts(max_distance + 1);
  for (const string &name : names) {
    auto iter = fstdxes_.find(name);
    if (iter == fstdxes_.end()) {
      LOG_ERROR("FstdxSearcher: name [{}] not found", name);
    } else {
      vector<vector<unique_ptr<string>>> res =
          iter->second->prefix_distance_search(word, max_distance);
      for (size_t i = 0; i < res.size(); ++i) {
        counts[i] += res[i].size();
        count += res[i].size();
      }
      results.emplace_back(std::move(res));
    }
  }
  return uniq_sort_results(std::move(results), counts, count);
}

std::vector<std::string> FstdxSearcher::uniq_sort_results(
    std::vector<std::vector<std::unique_ptr<std::string>>> &&results,
    size_t count) const {
  vector<string> result;
  if (count == 0) { return result; }
  vector<unique_ptr<string>> result_ptrs;
  result.reserve(count);
  result_ptrs.reserve(count);
  for (auto &res : results) {
    for (auto &ptr : res) {
      result_ptrs.emplace_back(std::move(ptr));
    }
  }
  std::sort(result_ptrs.begin(), result_ptrs.end(),
            [&](auto &x, auto &y) { return *x < *y; });

  // unique
  size_t index = 0;
  for (size_t i = 1; i < result_ptrs.size(); i++) {
    auto &x = result_ptrs[index];
    auto &y = result_ptrs[i];
    if (!(x->size() == y->size() && *x == *y)) {
      result.emplace_back(std::move(*result_ptrs[index]));
      index = i;
    }
  }
  // handle the last key
  result.emplace_back(std::move(*result_ptrs[index]));
  return result;
}

std::pair<std::vector<std::string>, std::string>
FstdxSearcher::regex_search(std::string_view pattern,
                            const std::vector<std::string> &names) const {
  vector<vector<unique_ptr<string>>> results;
  results.reserve(names.size());
  size_t count = 0;
  for (const auto &name : names) {
    auto iter = fstdxes_.find(name);
    if (iter == fstdxes_.end()) {
      LOG_ERROR("FstdxSearcher::regex_search, name [{}] not found", name);
    } else {
      pair<vector<unique_ptr<string>>, string> res =
          iter->second->regex_search(pattern, *thread_pool_ptr);
      if (!res.second.empty()) {
        return {vector<string>(), res.second};
      } else {
        count += res.first.size();
        results.emplace_back(std::move(res.first));
      }
    }
  }
  return {uniq_sort_results(std::move(results), count), ""};
}

void FstdxSearcher::insert_if_not_exists(const std::string &name,
                                         const std::string &fstdx_path) {
  if (fstdxes_.find(name) != fstdxes_.end()) { return; }
  insert(name, fstdx_path);
}

bool FstdxSearcher::insert(const std::string &name,
                           const std::string &fstdx_path) {
  if (fstdxes_.find(name) != fstdxes_.end()) {
    LOG_ERROR("Insert fstdx failed, as name {} already exists", name);
    return false;
  }

  shared_ptr<FstdxReader> ptr = make_shared<FstdxReader>(fstdx_path);

  // shared_ptr<FstdxReader> ptr = nullptr;
  // if (fst_indexes_names_set_.find(name) != fst_indexes_names_set_.end()) {
  //   ptr = std::static_pointer_cast<FstdxReader>(
  //       std::make_shared<FstdxHashReader>(fstdx_path));
  // } else {
  //   ptr = make_shared<FstdxReader>(fstdx_path);
  // }

  if (!(*ptr)) {
    LOG_ERROR("Insert fstdx failed, as path {} is not a valid fstdx file",
              fstdx_path);
    return false;
  }
  fstdxes_[name] = ptr;
  meta_json_["fstdx"][name] = fs::absolute(fstdx_path).string();

  const vector<string> fstdds = find_fstdd(fs::path(fstdx_path).parent_path());
  fstdds_[name] = std::vector<std::shared_ptr<FstddReader>>();
  for (const string &fstdd : fstdds) {
    auto ptr = std::make_shared<FstddReader>(fstdd);
    if (*(ptr)) { fstdds_[name].push_back(ptr); }
  }
  return true;
}

std::vector<std::string>
FstdxSearcher::find_fstdd(const std::string &target_dir) const {
  std::vector<std::string> fstdd_files;
  try {
    for (const auto &entry : fs::directory_iterator(target_dir)) {
      if (entry.is_regular_file()) {
        const fs::path &file_path = entry.path();
        if (file_path.extension() == ".fstdd") {
          fstdd_files.push_back(file_path.string());
        }
      }
    }
  } catch (const fs::filesystem_error &e) {
    LOG_ERROR("Found fstdd failed: {}", e.what());
    return fstdd_files;
  }
  return fstdd_files;
}

bool FstdxSearcher::save_to_disk(const std::string &meta_json_path) {
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

bool FstdxSearcher::build_fst_index(const std::string &fst_index_path) {
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
  sort(indices.begin(), indices.end(),
       [&](size_t i, size_t j) { return v_input[i].first < v_input[j].first; });
  vector<pair<string, fst::uint64bit>> sorted_input;
  sorted_input.reserve(v_input.size());
  for (size_t i : indices) {
    sorted_input.emplace_back(std::move(v_input[i].first), v_input[i].second);
  }

  ofstream of("view_index.txt");
  for (const auto &p : sorted_input) {
    of << p.first << ": " << p.second.bits << endl;
  }
  DyBlockProgBars dy_bars;
  auto refresh_bar = dy_bars.push_back(
      sorted_input.size(), "Compiling FST index:", indicators::Color::white);
  if (!fstd::compile_fst(sorted_input, fst_str_stream, true, true,
                         refresh_bar)) {
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

bool FstdxSearcher::load_file(const std::string &meta_json_path) {
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

  try {
    const std::string fst_index_path =
        meta_json_["fst_index"]["path"].get<std::string>();
    if (!load_fst_index(fst_index_path)) {
      LOG_ERROR("Failed to load FST index file {}", fst_index_path);
      return false;
    }
    fst_indexes_size_ = meta_json_["fst_index"]["size"].get<size_t>();
    fst_indexes_names_ = meta_json_["fst_index"]["names"].get<vector<string>>();
    fst_indexes_names_set_ = std::set<std::string>(fst_indexes_names_.begin(),
                                                   fst_indexes_names_.end());
  } catch (const json::exception &e) {
    LOG_ERROR("Meta JSON format error: {}", e.what());
    return false;
  } catch (const std::exception &e) {
    LOG_ERROR("Meta JSON processing error: {}", e.what());
    return false;
  }

  const json &fstdx_obj = meta_json_["fstdx"];
  for (auto it = fstdx_obj.begin(); it != fstdx_obj.end(); ++it) {
    const string &name = it.key();
    const string &path = it.value();
    if (!insert(name, path)) {
      LOG_ERROR("Failed to insert fstdx with name {} and path {}", name, path);
      return false;
    }
  }
  return true;
}

bool FstdxSearcher::save_fst_index_to_disk(const std::string &fst_index_path) {
  std::ofstream ofs(fst_index_path, std::ios::binary);
  if (!ofs) {
    LOG_ERROR("Cannot open the file: {}", fst_index_path);
    return false;
  }
  auto ptr = fst_indexes_searcher_.get_fst_byte_code();
  meta_json_["fst_index"]["byte_size"] = ptr->size();
  std::vector<char> dst;
  if (!compress_to_buffer(ptr->data(), ptr->size(), dst, 5)) { return false; }
  ofs.write(dst.data(), dst.size());
  meta_json_["fst_index"]["path"] = fs::absolute(fst_index_path).string();
  return true;
}

bool FstdxSearcher::load_fst_index(const std::string &fst_index_path) {
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
  if (!decompress_to_buffer(src.data(), src.size(), original_size, dst)) {
    return false;
  }
  fst_indexes_searcher_ = FstMapSearcher<fst::uint64bit>(std::move(dst));
  return true;
}

} // namespace fstd

// int main(int argc, char *argv[]) {
//   if (argc < 2) {
//     LOG_ERROR("Usage: {} <meta_json_path>", argv[0]);
//     return 1;
//   }
//   std::cout << "argv[1]: " << argv[1] << std::endl;
//   if (string(argv[1]) == "load") {
//     bool is_valid = false;
//     fstd::FstdxSearcher view("searcher_meta.json", is_valid);
//     if (!is_valid) {
//       LOG_ERROR("Failed to load FstdxSearcher from meta JSON file");
//       return 1;
//     }
//     while (true) {
//       std::string word;
//       std::string cmd;
//       std::cout << "Enter command (search/predict/exit): " << std::endl;
//       std::cin >> cmd;

//       if (cmd == "search") {
//         std::cin >> word;
//         std::string name;
//         std::vector<std::string> names;
//         while (std::cin >> name) {
//           if (name == "end") { break; }
//           names.emplace_back(name);
//         }
//         std::unordered_map<std::string, std::vector<std::string>> results =
//             view.search(word, names);
//         for (const auto &p : results) {
//           std::cout << "name: " << p.first << "\n";
//           for (const auto &item : p.second) {
//             std::cout << item << "\n";
//           }
//         }
//       } else if (cmd == "predict") {
//         std::cin >> word;
//         std::string name;
//         std::vector<std::string> names;
//         while (std::cin >> name) {
//           if (name == "end") { break; }
//           names.emplace_back(name);
//         }
//         std::vector<std::string> results = view.predictive_search(word,
//         names); std::cout << "predictive search result:\n"; for (const auto
//         &item : results) {
//           std::cout << item << "\n";
//         }
//       } else if (cmd == "exit") {
//         break;
//       } else {
//         LOG_ERROR("Unknown command: {}", word);
//       }
//       if (cmd == "exit") { break; }
//     }
//   } else {
//     fstd::FstdxSearcher view;
//     for (int i = 1; i < argc; ++i) {
//       view.insert(std::to_string(i), argv[i]);
//     }
//     view.build_fst_index("searcher_fst_index.fstdxidx");
//     view.save_to_disk("searcher_meta.json");
//     return 0;
//   }
// }