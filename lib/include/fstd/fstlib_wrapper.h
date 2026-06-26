#pragma once

#include <fstd/thread_pool.h>
#include <fstlib/build_fst.h>
#include <fstlib/map.h>
#include <fstlib/writer.h>
#include <sstream>

namespace fstd {

template <typename output_t>
inline std::pair<fst::Result, size_t>
compile(const std::vector<std::pair<std::string, output_t>> &input,
        std::ostream &os, bool sorted, bool verbose,
        std::function<void(size_t)> progress) {
  fst::FstWriter<output_t, true> writer(os, true, false, verbose,
                                        [&](const auto &feeder) {
                                          for (const auto &[word, _] : input) {
                                            feeder(word);
                                          }
                                        });
  return fst::build_fst<output_t>(input, writer, true, sorted, true, progress);
}

void show_error_message(fst::Result result, size_t line);

template <typename output_t>
bool compile_fst(const std::vector<std::pair<std::string, output_t>> &input,
                 std::ostringstream &oss_out, bool opt_sorted, bool opt_verbose,
                 std::function<void(size_t)> progress = nullptr) {
  fst::Result result;
  size_t line;

  // std::tie(result, line) = fst::dot<uint64_t>(input, oss_out, false);
  std::tie(result, line) =
      fstd::compile(input, oss_out, opt_sorted, opt_verbose, progress);

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
    return matcher_ptr_->exact_match_search(word, output);
  }

  operator bool() const { return matcher_ptr_ != nullptr; }

  bool is_valid() { return matcher_ptr_ != nullptr; }

  std::vector<std::unique_ptr<std::string>>
  common_prefix_search(std::string_view word) const {
    std::vector<std::unique_ptr<std::string>> result;
    matcher_ptr_->common_prefix_search(word, [&](size_t len, const auto &_) {
      result.emplace_back(std::make_unique<std::string>(word.substr(0, len)));
    });
    return result;
  }

  size_t longest_prefix_len(std::string_view word) const {
    return matcher_ptr_->longest_prefix_len(word);
  }

  std::vector<std::unique_ptr<std::string>>
  predictive_search(std::string_view word) const {
    return matcher_ptr_->predictive_search(word);
  }

  std::vector<std::unique_ptr<std::string>>
  edit_distance_search(std::string_view word, size_t max_edits,
                       size_t insert_cost = 1, size_t delete_cost = 1,
                       size_t replace_cost = 2) const {
    return matcher_ptr_->edit_distance_search(word, max_edits, insert_cost,
                                              delete_cost, replace_cost);
  }

  std::pair<std::vector<std::unique_ptr<std::string>>, std::string>
  regex_search(std::string_view pattern) const {
    return matcher_ptr_->regex_search(pattern);
  }

  std::pair<std::vector<std::unique_ptr<std::string>>, std::string>
  regex_search(std::string_view pattern, ThreadPool &thread_pool) const {
    return matcher_ptr_->regex_search(pattern, thread_pool);
  }

  std::vector<std::unique_ptr<std::pair<double, std::string>>>
  suggest(std::string_view word) const {
    return matcher_ptr_->suggest(word);
  }

  std::vector<std::vector<std::unique_ptr<std::string>>> prefix_distance_search(
      std::string_view sv, size_t max_distance, const size_t longest_prefix_len,
      const std::shared_ptr<std::set<std::string>> &prior_suffixes) const {
    return matcher_ptr_->prefix_distance_search(
        sv, max_distance, longest_prefix_len, prior_suffixes);
  }

  // std::vector<std::unique_ptr<std::pair<double, std::string>>>
  // spellcheck_word(std::string_view word, size_t n = 10) const {
  //   std::vector<std::tuple<double, std::string, output_t>> result;
  //   for (const auto &item : matcher_ptr_->suggest(word)) {
  //     if (n == 0) { break; }
  //     auto similarity = std::get<0>(item);
  //     const std::string &candidate = std::get<1>(item);
  //     const output_t &output = std::get<2>(item);
  //     result.emplace_back(similarity, candidate, output);
  //     n--;
  //   }
  //   return result;
  // }

  std::vector<std::pair<std::string, output_t>>
  enumerate(const size_t predictive_fst_key_size = 0,
            std::function<void(size_t)> progress = nullptr) const {
    std::vector<std::pair<std::string, output_t>> result;
    result.reserve(predictive_fst_key_size);
    matcher_ptr_->enumerate(
        [&](const std::string &word, const output_t output) {
          result.emplace_back(std::move(word), output);
          if (progress) { progress(result.size()); }
        });
    return result;
  }

private:
  std::shared_ptr<const std::vector<char>> byte_code_ptr_;
  std::shared_ptr<const fst::map<output_t>> matcher_ptr_;
};

} // namespace fstd