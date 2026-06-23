//
//  fstlib.h
//
//  Copyright (c) 2022 Yuji Hirose. All rights reserved.
//  MIT License
//

#pragma once

#include <fstlib/automaton.h>
#include <fstlib/matcher.h>

namespace fst {

//-----------------------------------------------------------------------------
// map
//-----------------------------------------------------------------------------

template <typename output_t> class map : public matcher<output_t> {
public:
  map(const char *byte_code, size_t byte_code_size)
      : matcher<output_t>(byte_code, byte_code_size) {}

  template <typename T>
  map(const T &byte_code)
      : matcher<output_t>(byte_code.data(), byte_code.size()) {}

  static const bool has_output = true;

  output_t operator[](std::string_view sv) const { return at(sv); }

  output_t operator[](const char *s) const { return at(s); }

  output_t at(std::string_view sv) const {
    auto output = output_t{};
    auto ret = matcher<output_t>::match(sv.data(), sv.size(),
                                        [&](const auto &_) { output = _; });
    if (!ret) { throw std::out_of_range("invalid key..."); }
    return output;
  }

  bool exact_match_search(std::string_view sv, output_t &output) const {
    return matcher<output_t>::match(sv.data(), sv.size(),
                                    [&](const auto &_) { output = _; });
  }

  bool common_prefix_search(
      std::string_view sv,
      std::function<void(size_t, const output_t &)> prefixes) const {
    return matcher<output_t>::match(sv.data(), sv.size(), nullptr, prefixes);
  }

  std::vector<std::pair<size_t, output_t>>
  common_prefix_search(std::string_view sv) const {
    std::vector<std::pair<size_t, output_t>> ret;
    common_prefix_search(sv, [&](size_t length, const output_t &output) {
      ret.emplace_back(std::pair(length, output));
    });
    return ret;
  }

  size_t longest_common_prefix_search(std::string_view sv,
                                      output_t &output) const {
    size_t prefix_len = 0;
    common_prefix_search(sv, [&](size_t len, const auto &_output) {
      prefix_len = len;
      output = _output;
    });
    return prefix_len;
  }

  size_t longest_common_prefix_search(std::string_view sv) const {
    return matcher<output_t>::longest_prefix_len(sv);
  }

  bool predictive_search(
      std::string_view sv,
      std::function<void(const std::string &, const output_t &)> callback,
      uint64_t mask = 0) const {
    auto ret = false;
    matcher<output_t>::depth_first_visit(
        matcher<output_t>::header_.start_address, std::string(), output_t{},
        DummyAutomaton(),
        [&](const auto &word, const auto &output) {
          ret = true;
          callback(word, output);
        },
        sv, mask);
    return ret;
  }

  std::vector<std::pair<std::string, output_t>>
  predictive_search(std::string_view sv, uint64_t mask = 0) const {
    std::vector<std::pair<std::string, output_t>> ret;
    predictive_search(
        sv,
        [&](const auto &word, const auto &output) {
          ret.emplace_back(word, output);
        },
        std::string_view(), mask);
    return ret;
  }

  std::vector<std::pair<std::string, output_t>>
  edit_distance_search(std::string_view sv, size_t max_edits,
                       size_t insert_cost = 1, size_t delete_cost = 1,
                       size_t replace_cost = 1, uint64_t mask = 0) const {

    std::vector<std::pair<std::string, output_t>> ret;

    if (sv.empty()) { return ret; }

    matcher<output_t>::depth_first_visit(
        matcher<output_t>::header_.start_address, std::string(), output_t{},
        LevenshteinAutomaton(sv, max_edits, insert_cost, delete_cost,
                             replace_cost),
        [&](const auto &word, const auto &output) {
          ret.emplace_back(std::pair(word, output));
        },
        std::string_view(), mask);

    return ret;
  }

  std::vector<std::vector<std::pair<std::string, output_t>>>
  prefix_distance_search(std::string_view sv, size_t max_distance,
                         uint64_t mask = 0) const {
    std::vector<std::vector<std::pair<std::string, output_t>>> ret(
        max_distance, std::vector<std::pair<std::string, output_t>>());
    if (sv.empty()) { return ret; }
    size_t longest_prefix_len = matcher<output_t>::longest_prefix_len(sv);
    std::cout << "sv.size(): " << sv.size() << std::endl;
    std::cout << "longest_prefix_len: " << longest_prefix_len << std::endl;
    matcher<output_t>::depth_first_visit(
        matcher<output_t>::header_.start_address, std::string(), output_t{},
        PrefixDistanceAutomaton(sv, max_distance, longest_prefix_len),
        [&](const auto &word, const auto &output, const auto &automaton) {
          ret[automaton.distance()].emplace_back(word, output);
        },
        std::string_view(), mask);

    return ret;
  }

  std::pair<std::vector<std::pair<std::string, output_t>>, std::string>
  regex_search(const std::string_view &pattern, uint64_t mask = 0) const {
    std::vector<std::pair<std::string, output_t>> results;

    std::string error_message;
    RegexAutomaton automaton(pattern, error_message);
    if (!error_message.empty()) { return {results, error_message}; }

    matcher<output_t>::depth_first_visit(
        matcher<output_t>::header_.start_address, // start from root node
        std::string(),                            // initial empty string
        output_t(),                               // initial empty output
        automaton,                                // regex automaton
        [&](const std::string &word, const output_t &output) {
          // callback when match success
          results.emplace_back(word, output);
        },
        std::string_view(), mask);

    return {results, error_message};
  }

  template <typename ThreadPool>
  std::pair<std::vector<std::pair<std::string, output_t>>, std::string>
  regex_search(const std::string_view &pattern, ThreadPool &thread_pool,
               uint64_t mask = 0) const {
    std::vector<std::pair<std::string, output_t>> results;

    std::string error_message;
    RegexAutomaton automaton(pattern, error_message);
    if (!error_message.empty()) { return {results, error_message}; }

    matcher<output_t>::depth_first_visit(
        matcher<output_t>::header_.start_address, // start from root node
        std::string(),                            // initial empty string
        output_t(),                               // initial empty output
        automaton,                                // regex automaton
        [&](const std::string &word, const output_t &output) {
          // callback when match success
          results.emplace_back(word, output);
        },
        thread_pool, std::string_view(), mask);

    return {results, error_message};
  }

  std::vector<std::tuple<double, std::string, output_t>>
  suggest(std::string_view word, uint64_t mask = 0) const {
    return matcher<output_t>::suggest_core(word, *this, mask);
  }

  template <typename T> void enumerate(T callback) const {
    matcher<output_t>::depth_first_visit(
        matcher<output_t>::header_.start_address, std::string(), output_t{},
        DummyAutomaton(), callback);
  }
};

} // namespace fst