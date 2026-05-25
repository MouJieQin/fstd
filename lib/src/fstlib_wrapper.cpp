#pragma once
#include "fstlib_wrapper.h"
#include "logger.hpp"
#include <tuple>

namespace fstd {

inline std::pair<fst::Result, size_t>
compile(const std::vector<std::pair<std::string, uint64_t>> &input,
        std::ostream &os, bool sorted, bool verbose) {
  fst::FstWriter<uint64_t, true> writer(os, true, false, verbose,
                                        [&](const auto &feeder) {
                                          for (const auto &[word, _] : input) {
                                            feeder(word);
                                          }
                                        });
  return fst::build_fst<uint64_t>(input, writer, true, sorted);
}

void show_error_message(fst::Result result, size_t line) {
  std::string error_message;

  switch (result) {
  case fst::Result::EmptyKey: error_message = "empty key"; break;
  case fst::Result::UnsortedKey: error_message = "unsorted key"; break;
  case fst::Result::DuplicateKey: error_message = "duplicate key"; break;
  default: error_message = "Unknown"; break;
  }
  LOG_ERROR("line {}: {}", line, error_message);
}

bool compile_fst(std::vector<std::pair<std::string, uint64_t>> &input,
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

} // namespace fstd