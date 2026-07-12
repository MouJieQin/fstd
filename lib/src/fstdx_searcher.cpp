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
  prior_suffixes_ = make_shared<std::map<string, size_t>>();
  prior_suf_lens_ = make_shared<std::set<size_t>>();
  thread_pool_ptr = make_shared<ThreadPool>(worker_num);
}

FstdxSearcher::FstdxSearcher(const std::string &meta_json_path,
                             size_t worker_num) {
  prior_suffixes_ = make_shared<std::map<string, size_t>>();
  prior_suf_lens_ = make_shared<std::set<size_t>>();
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

  fs::path default_dst_dir =
      fs::absolute(fs::path(iter->get<std::string>())).parent_path() / "data";
  return extract(name, file_path, default_dst_dir.string());
}

bool FstdxSearcher::contains(std::string_view word,
                             const std::vector<std::string> &names) const {
  for (const string &name : names) {
    auto iter = fstdxes_.find(name);
    if (iter == fstdxes_.end()) {
      LOG_ERROR("FstdxSearcher: name [{}] not found", name);
    } else {
      if (iter->second->contains(word)) { return true; }
    }
  }
  return false;
}

std::vector<std::string>
FstdxSearcher::exact_match_search(std::string_view word,
                                  const std::string &name) const {
  std::vector<std::string> result;
  auto iter = fstdxes_.find(name);
  if (iter == fstdxes_.end()) {
    LOG_ERROR("FstdxSearcher: name [{}] not found", name);
  } else {
    iter->second->exact_match_search(word, result);
  }
  return result;
}

std::unordered_map<std::string, std::vector<std::string>>
FstdxSearcher::exact_match_search(std::string_view word,
                                  const std::vector<std::string> &names) const {
  std::unordered_map<std::string, std::vector<std::string>> results;
  for (const string &name : names) {
    vector<string> result;
    auto iter = fstdxes_.find(name);
    if (iter == fstdxes_.end()) {
      LOG_ERROR("FstdxSearcher: name [{}] not found", name);
    } else {
      bool res = iter->second->exact_match_search(word, result);
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

size_t
FstdxSearcher::longest_prefix_len(std::string_view word,
                                  const std::vector<std::string> &names) const {
  size_t longest_len = 0;
  for (const string &name : names) {
    auto iter = fstdxes_.find(name);
    if (iter == fstdxes_.end()) {
      LOG_ERROR("FstdxSearcher: name {} not found", name);
    } else {
      size_t len = iter->second->longest_prefix_len(word);
      if (len > longest_len) { longest_len = len; }
    }
  }
  return longest_len;
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
      LOG_ERROR("FstdxSearcher: name [{}] not found", name);
    } else {
      vector<unique_ptr<string>> res =
          iter->second->edit_distance_search(word, edit_distance);
      count += res.size();
      results.emplace_back(std::move(res));
    }
  }
  return uniq_sort_results(std::move(results), count);
}

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
    const std::vector<size_t> &counts, size_t count) const {
  vector<string> result;
  if (count == 0) { return result; }
  size_t distance = results[0].size();
  vector<vector<unique_ptr<string>>> result_ptrs(distance);
  result.reserve(count);
  for (auto &res : results) {
    for (size_t i = 0; i < distance; i++) {
      result_ptrs[i].reserve(counts[i]);
      for (auto &ptr : res[i]) {
        result_ptrs[i].emplace_back(std::move(ptr));
      }
      std::sort(
          result_ptrs[i].begin(), result_ptrs[i].end(),
          [&](auto &x, auto &y) { return same_prefix_distance_cmp(*x, *y); });
    }
  }
  // unique
  for (auto &res_ptr : result_ptrs) {
    size_t index = 0;
    for (size_t i = 1; i < res_ptr.size(); ++i) {
      auto &x = res_ptr[index];
      auto &y = res_ptr[i];
      if (!(x->size() == y->size() && *x == *y)) {
        result.emplace_back(std::move(*x));
        index = i;
      }
    }
    if (!res_ptr.empty()) { result.emplace_back(std::move(*res_ptr[index])); }
  }
  return result;
}

int FstdxSearcher::has_prior_suffix(const std::string &word) const {
  for (auto it = prior_suf_lens_->crbegin(); it != prior_suf_lens_->crend();
       ++it) {
    size_t len = *it;
    if (word.size() >= len) {
      string suf = word.substr(word.size() - len, len);
      auto iter = prior_suffixes_->find(suf);
      if (iter != prior_suffixes_->end()) { return iter->second; }
    }
  }
  return -1;
}

bool FstdxSearcher::same_prefix_distance_cmp(const std::string &x,
                                             const std::string &y) const {
  int x_index = has_prior_suffix(x);
  int y_index = has_prior_suffix(y);
  if (x_index != -1) {
    if (y_index != -1) {
      return x_index != y_index
                 ? x_index < y_index
                 : (x.size() == y.size() ? x < y : x.size() > y.size());
    } else {
      return true;
    }
  } else {
    if (y_index != -1) {
      return false;
    } else {
      return x.size() == y.size() ? x < y : x.size() > y.size();
    }
  }
}

std::vector<std::string>
FstdxSearcher::prefix_distance_search(std::string_view word,
                                      const std::vector<std::string> &names,
                                      size_t max_distance) const {
  vector<vector<vector<unique_ptr<string>>>> results;
  results.reserve(names.size());
  size_t count = 0;
  vector<size_t> counts(max_distance + 1);

  size_t common_prefix_len = longest_prefix_len(word, names);

  for (const string &name : names) {
    auto iter = fstdxes_.find(name);
    if (iter == fstdxes_.end()) {
      LOG_ERROR("FstdxSearcher: name [{}] not found", name);
    } else {
      std::vector<std::vector<std::unique_ptr<std::string>>> res =
          iter->second->prefix_distance_search(
              word, max_distance, common_prefix_len, prior_suffixes_);
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

void FstdxSearcher::insert_prior_suffix(const std::vector<std::string> &sufs) {
  size_t index = prior_suffixes_->size();
  for (const string &suf : sufs) {
    auto p = prior_suffixes_->emplace(suf, index);
    if (p.second) {
      index += 1;
      prior_suf_lens_->insert(suf.size());
    }
  }
}

void FstdxSearcher::insert_if_not_exists(const std::string &name,
                                         const std::string &fstdx_path) {
  if (fstdxes_.find(name) != fstdxes_.end()) { return; }
  insert(name, fstdx_path);
}

bool FstdxSearcher::insert(const std::string &name,
                           const std::string &fstdx_path) {
  if (fstdxes_.find(name) != fstdxes_.end()) {
    LOG_ERROR("Insert fstdx failed, as name [{}] already exists", name);
    return false;
  }

  shared_ptr<FstdxReader> ptr = make_shared<FstdxReader>(fstdx_path);
  if (!(*ptr)) {
    LOG_ERROR("Insert fstdx failed. Invalid or corrupted .fstdx file: {}",
              fstdx_path);
    return false;
  }
  fstdxes_[name] = ptr;
  meta_json_["fstdx"][name] = fs::absolute(fstdx_path).string();

  const vector<string> fstdds =
      find_fstdd(fs::absolute(fs::path(fstdx_path)).parent_path().string());
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
    std::filesystem::path path_obj(
        reinterpret_cast<const char8_t *>(target_dir.c_str()));
    for (const auto &entry : fs::directory_iterator(path_obj)) {
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
  std::filesystem::path path_obj(
      reinterpret_cast<const char8_t *>(meta_json_path.c_str()));
  ofstream ofs(path_obj, ios::out);
  if (!ofs) {
    LOG_ERROR("Cannot open the file: {}", path_obj.string());
    return false;
  }
  ofs << meta_json_.dump(2);
  if (!ofs) {
    LOG_ERROR("Save meta json file {} failed", meta_json_path);
    return false;
  }
  return true;
}

bool FstdxSearcher::load_file(const std::string &meta_json_path) {
  std::filesystem::path path_obj(
      reinterpret_cast<const char8_t *>(meta_json_path.c_str()));
  ifstream ifs(path_obj);
  if (!ifs) {
    LOG_ERROR("Failed to open file {} for reading.", path_obj.string());
    return false;
  }
  json meta_json;
  try {
    ifs >> meta_json;
  } catch (const json::exception &e) {
    LOG_ERROR("Meta JSON file {} format error: {}", meta_json_path, e.what());
    return false;
  } catch (const std::exception &e) {
    LOG_ERROR("Meta JSON file {} read error: {}", meta_json_path, e.what());
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
      LOG_ERROR("Failed to insert fstdx with name {} and path {}", name, path);
      return false;
    }
  }

  if (meta_json_.contains("prior_suffix")) {
    const json &priori_suf_obj = meta_json_["prior_suffix"];
    if (!priori_suf_obj.is_object()) {
      LOG_ERROR("Meta JSON contain a valid 'prior_suffix' but it is not an "
                "object.");
      return false;
    }
    for (auto it = priori_suf_obj.begin(); it != priori_suf_obj.end(); ++it) {
      const string &lang = it.key();
      const auto &suffixes = it.value();
      if (!suffixes.is_array()) {
        LOG_ERROR("The prior suffexes of language: {} is not an array.", lang);
        return false;
      }
      insert_prior_suffix(suffixes.get<std::vector<std::string>>());
    }
  }
  return true;
}

} // namespace fstd
