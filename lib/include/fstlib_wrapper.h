#pragma once

#include "fstlib.h"
#include <sstream>

namespace fstd {

inline std::pair<fst::Result, size_t>
compile(const std::vector<std::pair<std::string, uint64_t>> &input,
        std::ostream &os, bool sorted, bool verbose = false);

void show_error_message(fst::Result result, size_t line);

bool compile_fst(std::vector<std::pair<std::string, uint64_t>> &input,
                 std::ostringstream &oss_out, bool opt_sorted,
                 bool opt_verbose);

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

  //   bool common_prefix_search(
  //       string_view word,
  //       std::vector<std::pair<std::string, output_t>> &p_outputs) const {
  //     std::vector<std::pair<std::string, output_t>> tmp_outputs;
  //     ret =
  //         matcher.common_prefix_search(word, [&](size_t len, const auto
  //         &output) {
  //           tmp_outputs.emplace_back(word.substr(0, len), output);
  //         });
  //     p_outputs.swap(tmp_outputs);
  //     return ret;
  //   }

  //   bool longest_common_prefix_search(
  //       string_view word, std::pair<std::string, output_t> &output_p) const {
  //     output_t output;
  //     auto len = matcher.longest_common_prefix_search(word, output);
  //     if (len > 0) { output_p = {word.substr(0, len), output}; }
  //     return len > 0;
  //   }

private:
  std::shared_ptr<const fst::map<output_t>> matcher_ptr;
  //   const fst::map<output_t> &matcher;
};

} // namespace fstd