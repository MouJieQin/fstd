//
//  fstlib.h
//
//  Copyright (c) 2022 Yuji Hirose. All rights reserved.
//  MIT License
//

#pragma once

namespace fst {

//-----------------------------------------------------------------------------
// set
//-----------------------------------------------------------------------------

class set : public matcher<none_t> {
public:
  set(const char *byte_code, size_t byte_code_size)
      : matcher<none_t>(byte_code, byte_code_size) {}

  template <typename T>
  set(const T &byte_code)
      : matcher<none_t>(byte_code.data(), byte_code.size()) {}

  static const bool has_output = false;

  bool common_prefix_search(std::string_view sv,
                            std::function<void(size_t)> prefixes) const {
    return matcher<none_t>::match(
        sv.data(), sv.size(), nullptr,
        [&](size_t len, const none_t &) { prefixes(len); });
  }

  std::vector<size_t> common_prefix_search(std::string_view sv) const {
    std::vector<size_t> ret;
    common_prefix_search(sv, [&](size_t length) { ret.push_back(length); });
    return ret;
  }

  size_t longest_prefix_len(std::string_view sv) const {
    size_t prefix_len = 0;
    common_prefix_search(sv, [&](size_t len) { prefix_len = len; });
    return prefix_len;
  }

  bool
  predictive_search(std::string_view sv,
                    std::function<void(const std::string &)> callback) const {
    auto ret = false;
    matcher<none_t>::depth_first_visit(
        matcher<none_t>::header_.start_address, std::string(), none_t{},
        DummyAutomaton(),
        [&](const auto &word, const auto &) {
          ret = true;
          callback(word);
        },
        sv);
    return ret;
  }

  std::vector<std::string> predictive_search(std::string_view sv) const {
    std::vector<std::string> ret;
    predictive_search(sv, [&](const auto &word) { ret.push_back(word); });
    return ret;
  }

  std::vector<std::string> edit_distance_search(std::string_view sv,
                                                size_t max_edits,
                                                size_t insert_cost = 1,
                                                size_t delete_cost = 1,
                                                size_t replace_cost = 1) const {

    std::vector<std::string> ret;

    if (sv.empty()) { return ret; }

    matcher<none_t>::depth_first_visit(
        matcher<none_t>::header_.start_address, std::string(), none_t{},
        LevenshteinAutomaton(sv, max_edits, insert_cost, delete_cost,
                             replace_cost),
        [&](const auto &word, const auto &) { ret.emplace_back(word); });

    return ret;
  }

  std::pair<std::vector<std::string>, std::string>
  regex_search(const std::string_view &pattern) const {
    std::vector<std::string> results;
    std::string error_message;
    RegexAutomaton automaton(pattern, error_message);
    if (!error_message.empty()) { return {results, error_message}; }
    matcher<none_t>::depth_first_visit(
        matcher<none_t>::header_.start_address, std::string(), none_t{},
        automaton, [&](const std::string &word, const none_t &) {
          results.emplace_back(word);
        });

    return {results, error_message};
  }

  std::vector<std::pair<double, std::string>>
  suggest(std::string_view word) const {
    return matcher<none_t>::suggest_core(word, *this);
  }

  template <typename T> void enumerate(T callback) const {
    matcher<none_t>::depth_first_visit(matcher<none_t>::header_.start_address,
                                       std::string(), none_t{},
                                       DummyAutomaton(), callback);
  }
};

} // namespace fst