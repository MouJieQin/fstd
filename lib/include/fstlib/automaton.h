//
//  fstlib.h
//
//  Copyright (c) 2022 Yuji Hirose. All rights reserved.
//  MIT License
//

#pragma once
#include <iostream>
#include <numeric>
#include <set>

#include <fstlib/matcher_utility.h>
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

namespace fst {

//-----------------------------------------------------------------------------
// PrefixDistanceAutomaton
//-----------------------------------------------------------------------------

class PrefixDistanceAutomaton {
public:
  PrefixDistanceAutomaton(
      std::string_view sv, const size_t max_distance,
      const size_t longest_prefix_len,
      const std::shared_ptr<std::set<std::string>> &prior_suffixes = nullptr)
      : s_(decode(sv)), max_distance_(max_distance),
        longest_prefix_len_(calc_c_len(sv.substr(0, longest_prefix_len))),
        common_prefix_len_(0), prefix_distance_(0), word_(""),
        prior_suffixes_(prior_suffixes), prior_suf_lens_(nullptr),
        max_prior_suf_len_(0) {
    if (prior_suffixes_) {
      prior_suf_lens_ = std::make_shared<std::set<size_t>>();
      for (const auto &suf : *prior_suffixes_) {
        prior_suf_lens_->insert(suf.size());
        size_t u8_len = calc_c_len(suf);
        if (u8_len > max_prior_suf_len_) { max_prior_suf_len_ = u8_len; }
      }
    }
  }

  PrefixDistanceAutomaton(const PrefixDistanceAutomaton &rhs) = default;

  void step(char c) {
    word_ += c;
    u8code_ += c;
    char32_t cp;
    if (!decode_codepoint(u8code_, cp)) { return; }
    u8code_.clear();

    if (prefix_distance_ != 0) {
      prefix_distance_ += 1;
      return;
    }

    if (s_[common_prefix_len_] == cp) {
      common_prefix_len_ += 1;
    } else {
      prefix_distance_ += 1;
    }
  }

  bool is_match() const {
    if (common_prefix_len_ == 0) {
      // one common prefix at least.
      return false;
    }
    return distance() <= max_distance_;
  }

  bool can_match() const {
    if (prefix_distance_ == 0) {
      return true;
    } else {
      if (common_prefix_len_ == 0) {
        return false;
      } else {
        return real_distance() < max_distance_ + max_prior_suf_len_;
      }
    }
  }

  size_t get_longest_prefix_len() const { return longest_prefix_len_; }

  size_t real_distance() const {
    return (longest_prefix_len_ - common_prefix_len_) + prefix_distance_;
  }

  size_t distance() const {
    size_t real_d = real_distance();
    if(prefix_distance_ == 0) { return real_d; }
    size_t prior_suf_len = has_prior_suffix(word_);
    if (real_d > prior_suf_len) {
      return real_d - prior_suf_len;
    } else {
      return 0;
    }
  }

private:
  size_t has_prior_suffix(const std::string &word) const {
    for (auto it = prior_suf_lens_->crbegin();it!=prior_suf_lens_->crend();++it) {
      auto len = *it;
      if (word.size() >= len) {
        std::string suf = word.substr(word.size() - len, len);
        if (prior_suffixes_->contains(suf)) { return calc_c_len(suf); }
      }
    }
    return 0;
  }

private:
  const std::u32string s_;
  const size_t max_distance_;
  const size_t longest_prefix_len_;
  size_t common_prefix_len_;
  size_t prefix_distance_;
  std::string u8code_;
  std::string word_;
  const std::shared_ptr<std::set<std::string>> prior_suffixes_;
  std::shared_ptr<std::set<size_t>> prior_suf_lens_;
  size_t max_prior_suf_len_;
};

//-----------------------------------------------------------------------------
// LevenshteinAutomaton
//-----------------------------------------------------------------------------

class LevenshteinAutomaton {
public:
  LevenshteinAutomaton(std::string_view sv, size_t max_edits,
                       size_t insert_cost, size_t delete_cost,
                       size_t replace_cost)
      : s_(decode(sv)), max_edits_(max_edits), insert_cost_(insert_cost),
        delete_cost_(delete_cost), replace_cost_(replace_cost) {
    state_.resize(s_.size() + 1);
    std::iota(state_.begin(), state_.end(), 0);
  }

  LevenshteinAutomaton(const LevenshteinAutomaton &rhs) = default;

  void step(char c) {
    u8code_ += c;
    char32_t cp;
    if (!decode_codepoint(u8code_, cp)) { return; }
    u8code_.clear();

    std::vector<size_t> new_state{state_[0] + 1};

    for (size_t i = 0; i < state_.size() - 1; i++) {
      auto cost = (s_[i] == cp) ? 0 : replace_cost_;
      auto edits = std::min({new_state[i] + insert_cost_, state_[i] + cost,
                             state_[i + 1] + delete_cost_});
      new_state.push_back(edits);
    }

    std::transform(
        new_state.begin(), new_state.end(), state_.begin(),
        [this](auto edits) { return std::min(edits, max_edits_ + 1); });
  }

  bool is_match() const {
    if (!u8code_.empty()) { return false; }
    return state_.back() <= max_edits_;
  }

  bool can_match() const {
    auto it = std::min_element(state_.begin(), state_.end());
    return *it <= max_edits_;
  }

private:
  std::u32string s_;
  size_t max_edits_;
  size_t insert_cost_;
  size_t delete_cost_;
  size_t replace_cost_; // TODO: better cost function is needed?
  std::vector<size_t> state_;
  std::string u8code_;
};

//-----------------------------------------------------------------------------
// DummyAutomaton
//-----------------------------------------------------------------------------

struct DummyAutomaton {
  void step(char _) {}
  bool is_match() const { return true; }
  bool can_match() const { return true; }
};

//-----------------------------------------------------------------------------
// RegexAutomaton
//-----------------------------------------------------------------------------

class Pcre2RegexAutomaton {
public:
  // Note: For FST traversal, it is highly recommended that your regex
  // patterns are anchored (e.g., "^pattern$"). Otherwise, the engine
  // assumes wildcards at the beginning and will never prune branches.
  explicit Pcre2RegexAutomaton(std::string_view pattern,
                               std::string &error_message) {
    int errornumber;
    PCRE2_SIZE erroroffset;
    error_message.clear();

    // Compile the regex. PCRE2_ANCHORED forces the pattern to match from
    // the start of the string, which is required for FST prefix pruning.
    const uint32_t compile_flags = PCRE2_ANCHORED | PCRE2_UTF | PCRE2_UCP;
    pcre2_code *re = pcre2_compile(reinterpret_cast<PCRE2_SPTR>(pattern.data()),
                                   pattern.size(), compile_flags, &errornumber,
                                   &erroroffset, nullptr);

    if (re == nullptr) {
      PCRE2_UCHAR buffer[256];
      pcre2_get_error_message(errornumber, buffer, sizeof(buffer));
      error_message =
          std::string("PCRE2 compilation failed: " +
                      std::string(reinterpret_cast<char *>(buffer)));
      return;
    }

    // Wrap the compiled AST in a shared_ptr. The FST traverses by copying
    // the automaton at branch points. We do NOT want to recompile the regex
    // for every node in the dictionary.
    re_ = std::shared_ptr<pcre2_code>(
        re, [](pcre2_code *p) { pcre2_code_free(p); });

    // CRITICAL PERFORMANCE GAIN: Enable Just-In-Time (JIT) compilation.
    // We must pass the partial flags during JIT compilation so that the JIT
    // compiler generates the specific machine code optimized for partial
    // matching.
    pcre2_jit_compile(re_.get(),
                      PCRE2_JIT_PARTIAL_HARD | PCRE2_JIT_PARTIAL_SOFT);

    // Allocate match data (the output block for PCRE2)
    match_data_ = std::shared_ptr<pcre2_match_data>(
        pcre2_match_data_create_from_pattern(re_.get(), nullptr),
        [](pcre2_match_data *m) { pcre2_match_data_free(m); });

    // // Evaluate the empty string initially to handle cases like "^$" or
    // "^a?$" evaluate();
  }

  // FSTs copy the automaton to explore different branches.
  // We share the compiled regex (thread-safe, read-only) but allocate
  // fresh match data for the new branch's state.
  Pcre2RegexAutomaton(const Pcre2RegexAutomaton &rhs)
      : re_(rhs.re_), buffer_(rhs.buffer_), u8code_(rhs.u8code_),
        is_match_(rhs.is_match_), can_match_(rhs.can_match_) {
    if (re_) {
      match_data_ = std::shared_ptr<pcre2_match_data>(
          pcre2_match_data_create_from_pattern(re_.get(), nullptr),
          [](pcre2_match_data *m) { pcre2_match_data_free(m); });
    }
  }

  void step(char c) {
    if (!can_match_) { return; }

    u8code_ += c;
    if (!u8_validator(u8code_)) { return; }
    buffer_ += u8code_;
    u8code_.clear();
    evaluate();
  }

  bool is_match() const { return is_match_; }

  bool can_match() const { return can_match_; }

private:
  std::shared_ptr<pcre2_code> re_;
  std::shared_ptr<pcre2_match_data> match_data_;
  std::string buffer_;
  std::string u8code_;
  bool is_match_ = false;
  bool can_match_ = true;

  void evaluate() {
    // 1. We always scan from 0 so lookarounds/anchors remain valid.
    // 2. We use PCRE2_PARTIAL_SOFT for accurate FST tree pruning.
    // 3. We use PCRE2_NO_UTF_CHECK to skip validation because step() guarantees
    // safety.
    const uint32_t match_flags = PCRE2_PARTIAL_SOFT | PCRE2_NO_UTF_CHECK;

    int rc = pcre2_match(
        re_.get(), reinterpret_cast<PCRE2_SPTR>(buffer_.data()), buffer_.size(),
        0,           // start offset
        match_flags, // CRITICAL: Enables partial prefix matching
        match_data_.get(), nullptr);

    if (rc >= 0) {
      // A full, valid match was found.
      is_match_ = true;
      // We set can_match_ to true because the string might still be
      // valid if extended (e.g. "hello" matching "^hello.*"). If the
      // next char breaks it, the subsequent evaluate() will prune it.
      can_match_ = true;
    } else if (rc == PCRE2_ERROR_PARTIAL) {
      // Partial Match found (valid prefix, can keep expanding)
      is_match_ = false;
      can_match_ = true;
    } else {
      // PCRE2_ERROR_NOMATCH or another fatal error.
      // The regex can no longer be satisfied. Prune this FST branch.
      is_match_ = false;
      can_match_ = false;
    }
  }
};

using RegexAutomaton = Pcre2RegexAutomaton;

} // namespace fst