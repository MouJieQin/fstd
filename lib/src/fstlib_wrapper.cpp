#pragma once
#include "fstlib_wrapper.h"
#include "logger.hpp"
#include <tuple>

using namespace std;
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

template <typename output_t> struct traits {
  static output_t convert(uint32_t n) {}
  static output_t convert(uint64_t n) {}
  static output_t convert(const string &s) {}
};

template <> struct traits<uint32_t> {
  static uint32_t convert(uint32_t n) { return n; }
  static uint32_t convert(uint64_t n) { return static_cast<uint64_t>(n); }
  static uint32_t convert(const string &s) { return stoi(s); }
};

template <> struct traits<string> {
  static string convert(uint32_t n) { return to_string(n); }
  static string convert(uint64_t n) { return to_string(n); }
  static string convert(const string &s) { return s; }
};

enum class SearchType {
  Spellcheck,
  Search,
  Prefix,
  Longest,
  Predictive,
  Fuzzy,
  Regex
};

template <typename T>
bool spellcheck_word(const T &matcher, const std::string word) {
  if (matcher.contains(word)) {
    cout << "correct word!" << std::endl;
    return true;
  }

  size_t count = 10;
  for (const auto &item : matcher.suggest(word)) {
    if (count == 0) { break; }
    auto similarity = std::get<0>(item);
    const auto &candidate = std::get<1>(item);
    cout << candidate << ": " << similarity << std::endl;
    count--;
  }
  return false;
}

// template <typename output_t> class FstMapSearcher {
// public:
//   FstMapSearcher(const vector<char> &byte_code)
//       : matcher(fst::map<output_t>(byte_code)) {}

//   bool exact_match_search(string_view word, output_t &output) const {
//     bool ret = matcher.exact_match_search(word, output);
//     return ret;
//   }

//   bool common_prefix_search(
//       string_view word,
//       std::vector<std::pair<std::string, output_t>> &p_outputs) const {
//     std::vector<std::pair<std::string, output_t>> tmp_outputs;
//     ret =
//         matcher.common_prefix_search(word, [&](size_t len, const auto &output) {
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

// private:
//   const fst::map<output_t> &matcher;
// };

// template <typename output_t, typename T, typename U>
// void map_search_word(const T &byte_code, SearchType search_type, bool verbose,
//                      const U &matcher, string_view word, size_t edit_distance) {
//   auto ret = false;
//   switch (search_type) {
//   case SearchType::Spellcheck: spellcheck_word(matcher, string(word)); return;

//   case SearchType::Longest:
//     output_t output;
//     auto len = matcher.longest_common_prefix_search(word, output);
//     if (len > 0) {
//       ret = true;
//       cout << word.substr(0, len) << ": " << output << endl;
//     }
//     break;
//   case SearchType::Predictive:
//     ret = matcher.predictive_search(word,
//                                     [&](const auto &word, const auto &output) {
//                                       cout << word << ": " << output << endl;
//                                     });
//   }
//   else if (cmd == "fuzzy") {
//     auto results = matcher.edit_distance_search(word, edit_distance, 1, 1, 2);
//     ret = !results.empty();
//     for (const auto &[word, output] : results) {
//       cout << word << ": " << output << endl;
//     }
//     break;
//   case SearchType::Regex:
//     auto p_results = matcher.regex_search(word);
//     const auto &results = p_results.first;
//     const auto &error_message = p_results.second;
//     if (!error_message.empty()) {
//       cerr << error_message << endl;
//       return;
//     }
//     ret = !results.empty();
//     for (const auto &[word, output] : results) {
//       cout << word << ": " << output << endl;
//     }
//     break;
//   }
//   if (!ret) { cout << "not found..." << endl; }
// }

// template <typename output_t, typename T>
// void map_search(const T &byte_code, SearchType search_type, bool verbose,
//                 string_view word, size_t edit_distance) {
//   fst::map<output_t> matcher(byte_code);
//   matcher.set_trace(verbose);

//   if (matcher) {
//     if (word.empty()) {
//       string word;
//       while (getline(cin, word)) {
//         map_search_word<output_t>(byte_code, search_type, verbose, matcher,
//                                   word, edit_distance);
//       }
//     } else {
//       map_search_word<output_t>(byte_code, search_type, verbose, matcher, word,
//                                 edit_distance);
//     }
//   } else {
//     cerr << "invalid file..." << endl;
//   }
// }

// template <typename T, typename U>
// void set_search_word(const T &byte_code, SearchType search_type, bool verbose,
//                      const U &matcher, string_view word, size_t edit_distance) {
//   bool ret = false;
//   switch (search_type) {
//   case SearchType::Spellcheck:
//     ret = spellcheck_word(matcher, string(word));
//     break;
//   case SearchType::Search:
//     ret = matcher.contains(word);
//     if (ret) { cout << "exist!" << endl; }
//     break;
//   case SearchType::Prefix:
//     ret = matcher.common_prefix_search(
//         word, [&](size_t len) { cout << word.substr(0, len) << endl; });
//     break;
//   case SearchType::Longest:
//     auto len = matcher.longest_common_prefix_search(word);
//     if (len > 0) {
//       ret = true;
//       cout << word.substr(0, len) << endl;
//     }
//     break;
//   case SearchType::Predictive:
//     ret = matcher.predictive_search(
//         word, [&](const auto &word) { cout << word << endl; });
//     break;
//   case SearchType::Fuzzy:
//     auto results = matcher.edit_distance_search(word, edit_distance, 1, 1, 2);
//     ret = !results.empty();
//     for (const auto &word : results) {
//       cout << word << endl;
//     }
//     break;
//   case SearchType::Regex:
//     auto p_results = matcher.regex_search(word);
//     const auto &results = p_results.first;
//     const auto &error_message = p_results.second;
//     if (!error_message.empty()) {
//       cerr << error_message << endl;
//       return;
//     }
//     ret = !results.empty();
//     for (const auto &word : results) {
//       cout << word << endl;
//     }
//     break;
//   case SearchType::Spellcheck:
//     ret = spellcheck_word(matcher, string(word));
//     break;
//   default: break;
//   }
//   if (!ret) { cerr << "not found..." << endl; }
// }

// template <typename T>
// void set_search(const T &byte_code, SearchType search_type, bool verbose,
//                 string_view word, size_t edit_distance) {
//   fst::set matcher(byte_code);
//   matcher.set_trace(verbose);

//   if (matcher) {
//     if (word.empty()) {
//       string word;
//       while (getline(cin, word)) {
//         set_search_word(byte_code, search_type, verbose, matcher, word,
//                         edit_distance);
//       }
//     } else {
//       set_search_word(byte_code, search_type, verbose, matcher, word,
//                       edit_distance);
//     }
//   } else {
//     cerr << "invalid file..." << endl;
//   }
// }

// template <typename T>
// void search(const T &byte_code, SearchType search_type, bool verbose,
//             string_view word, size_t edit_distance) {
//   auto type = fst::get_output_type(byte_code);

//   if (type == fst::OutputType::uint32_t) {
//     map_search<uint32_t>(byte_code, search_type, verbose, word, edit_distance);
//   } else if (type == fst::OutputType::uint64_t) {
//     map_search<uint64_t>(byte_code, search_type, verbose, word, edit_distance);
//   } else if (type == fst::OutputType::string) {
//     map_search<string>(byte_code, search_type, verbose, word, edit_distance);
//   } else if (type == fst::OutputType::none_t) {
//     set_search(byte_code, search_type, verbose, word, edit_distance);
//   }
// }

} // namespace fstd