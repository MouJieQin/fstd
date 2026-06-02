#pragma once

#include <fstd/fstlib.h>
#include <sstream>

namespace fstd {

// inline std::pair<fst::Result, size_t>
// compile(const std::vector<std::pair<std::string, uint64_t>> &input,
//         std::ostream &os, bool sorted, bool verbose = false);

// ------------------------------
// 通用：根据元素大小排序索引
// ------------------------------
template <typename Cont_p>
inline std::vector<size_t> sort_indexes(const Cont_p &input) {
  std::vector<size_t> indices(input.size());
  std::iota(indices.begin(), indices.end(), 0);
  std::sort(indices.begin(), indices.end(),
            [&](size_t i, size_t j) { return input[i] < input[j]; });
  return indices;
}

// ------------------------------
// 特化：针对 vector<pair<...>> 按 first 排序
// ------------------------------
template <typename T1, typename T2>
inline std::vector<size_t>
sort_indexes(const std::vector<std::pair<T1, T2>> &input) {
  std::vector<size_t> indices(input.size());
  std::iota(indices.begin(), indices.end(), 0);
  std::sort(indices.begin(), indices.end(), [&](size_t i, size_t j) {
    return input[i].first < input[j].first;
  });
  return indices;
}

// ------------------------------
// 通用版本：支持 pair 等结构
// ------------------------------
template <typename T1, typename T2>
inline std::vector<std::string>
sort_container(std::vector<std::pair<T1, T2>> &&input) {
  auto indices = sort_indexes(input);
  std::vector<std::string> res;
  res.reserve(indices.size());

  for (size_t i : indices) {
    res.emplace_back(std::move(input[i].first));
  }
  return res;
}

inline std::vector<std::string>
sort_container(std::vector<std::string> &&input) {
  auto indices = sort_indexes(input);
  std::vector<std::string> res;
  res.reserve(indices.size());

  for (size_t i : indices) {
    res.emplace_back(std::move(input[i]));
  }
  return res;
}

template <typename output_t>
inline std::pair<fst::Result, size_t>
compile(const std::vector<std::pair<std::string, output_t>> &input,
        std::ostream &os, bool sorted, bool verbose) {
  fst::FstWriter<output_t, true> writer(os, true, false, verbose,
                                        [&](const auto &feeder) {
                                          for (const auto &[word, _] : input) {
                                            feeder(word);
                                          }
                                        });
  return fst::build_fst<output_t>(input, writer, true, sorted);
}

void show_error_message(fst::Result result, size_t line);

template <typename output_t>
bool compile_fst(const std::vector<std::pair<std::string, output_t>> &input,
                 std::ostringstream &oss_out, bool opt_sorted,
                 bool opt_verbose) {
  fst::Result result;
  size_t line;

  // std::tie(result, line) = fst::dot<uint64_t>(input, oss_out, false);
  std::tie(result, line) =
      fstd::compile(input, oss_out, opt_sorted, opt_verbose);

  if (result == fst::Result::Success) { return true; }
  show_error_message(result, line);
  return false;
}

template <typename output_t> class FstMapSearcher {
public:
  FstMapSearcher() : matcher_ptr_(nullptr) {}
  FstMapSearcher(std::vector<char> &&byte_code)
      : byte_code_ptr_(
            std::make_shared<const std::vector<char>>(std::move(byte_code))),
        matcher_ptr_(std::make_shared<const fst::map<output_t>>(
            byte_code_ptr_->data(), byte_code_ptr_->size())) {}

  FstMapSearcher(const FstMapSearcher &rhs) {
    matcher_ptr_ = rhs.matcher_ptr_;
    byte_code_ptr_ = rhs.byte_code_ptr_;
  }
  void operator=(const FstMapSearcher &rhs) {
    matcher_ptr_ = rhs.matcher_ptr_;
    byte_code_ptr_ = rhs.byte_code_ptr_;
  }

  const std::shared_ptr<const std::vector<char>> &get_fst_byte_code() const {
    return byte_code_ptr_;
  }

  bool exact_match_search(std::string_view word, output_t &output) const {
    bool ret = matcher_ptr_->exact_match_search(word, output);
    return ret;
  }

  bool is_valid() { return matcher_ptr_ != nullptr; }

  std::vector<std::pair<std::string, output_t>>
  common_prefix_search(std::string_view word) const {
    std::vector<std::pair<std::string, output_t>> result;
    matcher_ptr_->common_prefix_search(
        word, [&](size_t len, const auto &output) {
          result.emplace_back(word.substr(0, len), output);
        });
    return result;
  }

  size_t
  longest_common_prefix_search(std::string_view word,
                               std::pair<std::string, output_t> &result) const {
    size_t len =
        matcher_ptr_->longest_common_prefix_search(word, result.second);
    if (len > 0) { result.first = word.substr(0, len); }
    return len;
  }

  std::vector<std::pair<std::string, output_t>>
  predictive_search(std::string_view word) const {
    std::vector<std::pair<std::string, output_t>> result;
    matcher_ptr_->predictive_search(word,
                                    [&](const auto &word, const auto &output) {
                                      result.emplace_back(word, output);
                                    });
    return result;
  }

  std::vector<std::pair<std::string, output_t>>
  edit_distance_search(std::string_view word, size_t max_edits,
                       size_t insert_cost = 1, size_t delete_cost = 1,
                       size_t replace_cost = 2) const {
    return matcher_ptr_->edit_distance_search(word, max_edits, insert_cost,
                                              delete_cost, replace_cost);
  }

  std::pair<std::vector<std::pair<std::string, output_t>>, std::string>
  regex_search(std::string_view pattern) const {
    return matcher_ptr_->regex_search(pattern);
  }

  std::vector<std::tuple<double, std::string, output_t>>
  suggest(std::string_view word) const {
    return fst::matcher<output_t>::suggest_core(word, *this);
  }

  std::vector<std::tuple<double, std::string, output_t>>
  spellcheck_word(std::string_view word, size_t n = 10) const {
    std::vector<std::tuple<double, std::string, output_t>> result;
    for (const auto &item : matcher_ptr_->suggest(word)) {
      if (n == 0) { break; }
      auto similarity = std::get<0>(item);
      const std::string &candidate = std::get<1>(item);
      const output_t &output = std::get<2>(item);
      result.emplace_back(similarity, candidate, output);
      n--;
    }
    return result;
  }

  std::vector<std::pair<std::string, output_t>>
  enumerate(const size_t predictive_fst_key_size = 0) const {
    std::vector<std::pair<std::string, output_t>> result;
    result.reserve(predictive_fst_key_size);
    matcher_ptr_->enumerate(
        [&](const std::string &word, const output_t output) {
          result.emplace_back(std::move(word), output);
        });
    return result;
  }

private:
  std::shared_ptr<const std::vector<char>> byte_code_ptr_;
  std::shared_ptr<const fst::map<output_t>> matcher_ptr_;
};

} // namespace fstd