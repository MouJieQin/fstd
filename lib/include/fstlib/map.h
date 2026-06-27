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

  bool contains(std::string_view sv) const {
    return matcher<output_t>::contains(sv);
  }

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

  std::vector<size_t> common_prefix_search(std::string_view sv) const {
    std::vector<std::pair<size_t, output_t>> ret;
    common_prefix_search(sv, [&](size_t length, const output_t &_) {
      ret.emplace_back(length);
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

  size_t longest_prefix_len(std::string_view sv) const {
    return matcher<output_t>::longest_prefix_len(sv);
  }

  std::vector<std::unique_ptr<std::string>>
  predictive_search(std::string_view sv) const {
    std::vector<std::unique_ptr<std::string>> ret;
    matcher<output_t>::depth_first_visit(
        matcher<output_t>::header_.start_address, std::string(), output_t{},
        DummyAutomaton(),
        [&](const auto &word, const auto &_) {
          ret.emplace_back(std::make_unique<std::string>(word));
        },
        sv);
    return ret;
  }

  std::vector<std::unique_ptr<std::string>>
  edit_distance_search(std::string_view sv, size_t max_edits,
                       size_t insert_cost = 1, size_t delete_cost = 1,
                       size_t replace_cost = 2) const {
    std::vector<std::unique_ptr<std::string>> ret;
    if (sv.empty()) { return ret; }
    matcher<output_t>::depth_first_visit(
        matcher<output_t>::header_.start_address, std::string(), output_t{},
        LevenshteinAutomaton(sv, max_edits, insert_cost, delete_cost,
                             replace_cost),
        [&](const auto &word, const auto &_) {
          ret.emplace_back(std::make_unique<std::string>(word));
        },
        std::string_view());

    return ret;
  }

  std::vector<std::vector<std::unique_ptr<std::string>>> prefix_distance_search(
      std::string_view sv, size_t max_distance, const size_t longest_prefix_len,
      const std::shared_ptr<std::set<std::string>> &prior_suffixes) const {
    std::vector<std::vector<std::unique_ptr<std::string>>> ret(max_distance +
                                                               1);
    if (sv.empty()) { return ret; }
    PrefixDistanceAutomaton atm(sv, max_distance, longest_prefix_len,
                                prior_suffixes);
    matcher<output_t>::depth_first_visit(
        matcher<output_t>::header_.start_address, std::string(), output_t{},
        atm,
        [&](const auto &word, const auto &_, const auto &automaton) {
          ret[automaton.distance()].emplace_back(
              std::make_unique<std::string>(word));
        },
        std::string_view());
    return ret;
  }

  std::pair<std::vector<std::unique_ptr<std::string>>, std::string>
  regex_search(std::string_view pattern) const {
    std::vector<std::unique_ptr<std::string>> results;
    std::string error_message;
    RegexAutomaton automaton(pattern, error_message);
    if (!error_message.empty()) {
      return {std::move(results), std::move(error_message)};
    }

    matcher<output_t>::depth_first_visit(
        matcher<output_t>::header_.start_address, // start from root node
        std::string(),                            // initial empty string
        output_t(),                               // initial empty output
        automaton,                                // regex automaton
        [&](const std::string &word, const output_t &_) {
          // callback when match success
          results.emplace_back(std::make_unique<std::string>(word));
        },
        std::string_view());

    return {std::move(results), std::move(error_message)};
  }

  template <typename ThreadPool>
  std::pair<std::vector<std::unique_ptr<std::string>>, std::string>
  regex_search(std::string_view pattern, ThreadPool &thread_pool) const {
    std::vector<std::unique_ptr<std::string>> results;

    std::string error_message;
    RegexAutomaton automaton(pattern, error_message);
    if (!error_message.empty()) {
      return {std::move(results), std::move(error_message)};
    }

    matcher<output_t>::depth_first_visit(
        matcher<output_t>::header_.start_address, // start from root node
        std::string(),                            // initial empty string
        output_t(),                               // initial empty output
        automaton,                                // regex automaton
        [&](const std::string &word, const output_t &_) {
          // callback when match success
          results.emplace_back(std::make_unique<std::string>(word));
        },
        thread_pool, std::string_view());

    return {std::move(results), std::move(error_message)};
  }

  std::vector<std::unique_ptr<std::pair<double, std::string>>>
  suggest(std::string_view word) const {
    return matcher<output_t>::suggest_core(word, *this);
  }

  template <typename T> void enumerate(T callback) const {
    matcher<output_t>::depth_first_visit(
        matcher<output_t>::header_.start_address, std::string(), output_t{},
        DummyAutomaton(), callback);
  }
};

} // namespace fst