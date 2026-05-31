#pragma once

#include <fstd/fstlib.h>
#include <sstream>

namespace fstd {

// inline std::pair<fst::Result, size_t>
// compile(const std::vector<std::pair<std::string, uint64_t>> &input,
//         std::ostream &os, bool sorted, bool verbose = false);

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
  FstMapSearcher() : matcher_ptr(nullptr) {}
  FstMapSearcher(const std::vector<char> &byte_code)
      : matcher_ptr(std::make_shared<const fst::map<output_t>>(byte_code)) {}

  FstMapSearcher(const FstMapSearcher &rhs) { matcher_ptr = rhs.matcher_ptr; }
  void operator=(const FstMapSearcher &rhs) { matcher_ptr = rhs.matcher_ptr; }

  bool exact_match_search(std::string_view word, output_t &output) const {
    bool ret = matcher_ptr->exact_match_search(word, output);
    return ret;
  }

  bool is_valid() { return matcher_ptr != nullptr; }

  std::vector<std::pair<std::string, output_t>>
  common_prefix_search(std::string_view word) const {
    std::vector<std::pair<std::string, output_t>> result;
    matcher_ptr->common_prefix_search(
        word, [&](size_t len, const auto &output) {
          result.emplace_back(word.substr(0, len), output);
        });
    return result;
  }

  size_t
  longest_common_prefix_search(std::string_view word,
                               std::pair<std::string, output_t> &result) const {
    size_t len = matcher_ptr->longest_common_prefix_search(word, result.second);
    if (len > 0) { result.first = word.substr(0, len); }
    return len;
  }

  std::vector<std::pair<std::string, output_t>>
  predictive_search(std::string_view word) const {
    std::vector<std::pair<std::string, output_t>> result;
    matcher_ptr->predictive_search(word,
                                   [&](const auto &word, const auto &output) {
                                     result.emplace_back(word, output);
                                   });
    return result;
  }

  std::vector<std::pair<std::string, output_t>>
  edit_distance_search(std::string_view word, size_t max_edits,
                       size_t insert_cost = 1, size_t delete_cost = 1,
                       size_t replace_cost = 2) const {
    return matcher_ptr->edit_distance_search(word, max_edits, insert_cost,
                                             delete_cost, replace_cost);
  }

  std::pair<std::vector<std::pair<std::string, output_t>>, std::string>
  regex_search(std::string_view pattern) const {
    return matcher_ptr->regex_search(pattern);
  }

  std::vector<std::tuple<double, std::string, output_t>>
  suggest(std::string_view word) const {
    return fst::matcher<output_t>::suggest_core(word, *this);
  }

  std::vector<std::tuple<double, std::string, output_t>>
  spellcheck_word(std::string_view word, size_t n = 10) const {
    std::vector<std::tuple<double, std::string, output_t>> result;
    for (const auto &item : matcher_ptr->suggest(word)) {
      if (n == 0) { break; }
      auto similarity = std::get<0>(item);
      const std::string &candidate = std::get<1>(item);
      const output_t &output = std::get<2>(item);
      result.emplace_back(similarity, candidate, output);
      n--;
    }
    return result;
  }

  std::vector<std::pair<std::string, output_t>> enumerate() const {
    std::vector<std::pair<std::string, output_t>> result;
    result.reserve(2000000);
    matcher_ptr->enumerate(
        [&](const std::string &word, const output_t output) {
          result.emplace_back(word, output);
        });
    return result;
  }

private:
  std::shared_ptr<const fst::map<output_t>> matcher_ptr;
};

} // namespace fstd