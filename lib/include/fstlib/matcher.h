//
//  fstlib.h
//
//  Copyright (c) 2022 Yuji Hirose. All rights reserved.
//  MIT License
//

#pragma once
#include <future>
#include <iostream>
#include <memory>
#include <mutex>

#include <fstlib/automaton.h>
#include <fstlib/matcher_utility.h>

namespace fst {

//-----------------------------------------------------------------------------
// matcher
//-----------------------------------------------------------------------------

template <typename output_t> class matcher {
public:
  using output_type = output_t;

  matcher(const char *byte_code, size_t byte_code_size)
      : byte_code_(byte_code), byte_code_size_(byte_code_size) {

    if (!header_.read(byte_code, byte_code_size)) { return; }

    if (static_cast<OutputType>(header_.flags.data.output_type) !=
        OutputTraits<output_t>::type()) {
      return;
    }

    is_valid_ = true;
  }

  operator bool() const { return is_valid_; }

  void set_trace(bool on) { trace_ = on; }

  bool contains(std::string_view sv) const {
    return matcher<output_t>::match(sv.data(), sv.size());
  }

protected:
  size_t longest_prefix_len(std::string_view sv) const {
    size_t longest_prefix_len;
    matcher<output_t>::match(sv.data(), sv.size(), longest_prefix_len);
    return longest_prefix_len;
  }

  bool match(
      const char *str, size_t len,
      std::function<void(const output_t &)> outputs = nullptr,
      std::function<void(size_t, const output_t &)> prefixes = nullptr) const {
    size_t longest_prefix_len;
    return match(str, len, longest_prefix_len, outputs, prefixes);
  }

  bool match(
      const char *str, size_t len, size_t &longest_prefix_len,
      std::function<void(const output_t &)> outputs = nullptr,
      std::function<void(size_t, const output_t &)> prefixes = nullptr) const {

    if (trace_) {
      std::cout << "Char\tAddress\tArc\tN F L\tNxtAddr\tOutput\tStOuts\tSize"
                << std::endl;
      std::cout << "----\t-------\t---\t-----\t-------\t------\t------\t----"
                << std::endl;
    }

    auto ret = false;
    auto output = output_t{};

    auto address = header_.start_address;
    auto i = 0u;
    auto arc_in_jump_table = false;
    while (i < len) {
      auto ch = static_cast<uint8_t>(str[i]);
      auto state_output = output_t{};

      auto end = byte_code_ + address;
      auto p = end;

      auto ope = FstOpe(*p--);

      if (ope.has_jump_table()) {
        auto jump_table_element_size = ope.jump_table_element_size();
        size_t jump_table_count = 0;
        auto vb_len = vb_decode_value_reverse(p, jump_table_count);
        p -= vb_len;
        p -= jump_table_count * jump_table_element_size;

        auto jump_table = p;

        if (header_.flags.data.jump_table_labels) {
          // The labels are stored contiguously next to the jump table, so
          // the binary search only touches sequential memory.
          auto labels =
              reinterpret_cast<const uint8_t *>(p) + 1 - jump_table_count;

          auto jump_table_byte_size =
              1 + vb_len + jump_table_count * jump_table_element_size +
              jump_table_count;

          auto found = lower_bound_index(
              0, jump_table_count, [&](auto i) { return labels[i] < ch; });

          if (found < jump_table_count && labels[found] == ch) {
            auto offset =
                lookup_jump_table(jump_table, found, jump_table_element_size);
            address -= offset + jump_table_byte_size;
            arc_in_jump_table = true;
          } else {
            break;
          }
          continue;
        }

        auto jump_table_byte_size =
            1 + vb_len + jump_table_count * jump_table_element_size;

        auto base_address = byte_code_ + address - jump_table_byte_size;

        auto get_arc = [&](auto i) -> uint8_t {
          auto p = base_address -
                   lookup_jump_table(jump_table, i, jump_table_element_size);
          auto ope = FstOpe(*p--);
          return read_arc(ope, p);
        };

        auto found = lower_bound_index(0, jump_table_count,
                                       [&](auto i) { return get_arc(i) < ch; });

        if (found < jump_table_count && get_arc(found) == ch) {
          auto offset =
              lookup_jump_table(jump_table, found, jump_table_element_size);
          address -= offset + jump_table_byte_size;
        } else {
          break;
        }
        continue;
      }

      uint8_t arc;
      if (arc_in_jump_table) {
        // The record was reached through a jump table hit, so its label is
        // already verified and the record itself carries no label byte.
        arc = ch;
        arc_in_jump_table = false;
      } else {
        arc = read_arc(ope, p);
      }

      uint32_t delta, hub_next_address;
      bool has_hub_next_address;
      read_delta(ope, p, delta, hub_next_address, has_hub_next_address);

      auto output_suffix = output_t{};
      if (ope.data.has_output) {
        p -= OutputTraits<output_t>::read_byte_value(p, output_suffix);
      }

      if (header_.need_state_output) {
        if (ope.data.has_state_output) {
          p -= OutputTraits<output_t>::read_byte_value(p, state_output);
        }
      }

      auto byte_size = std::distance(p, end);

      auto next_address = 0u;
      if (!ope.data.no_address) {
        if (has_hub_next_address) {
          next_address = hub_next_address;
        } else if (delta) {
          next_address = address - byte_size - delta + 1;
        }
      } else {
        next_address = address - byte_size;
      }

      if (trace_) {
        std::cout << char_to_string(ch) << "\t";
        std::cout << address << "\t";
        std::cout << arc << "\t";
        std::cout << (ope.data.no_address ? "↑" : " ") << ' '
                  << (ope.data.final ? '*' : ' ') << ' '
                  << (ope.data.last_transition ? "‾" : " ") << "\t";

        // Next Address
        if (next_address) {
          std::cout << next_address;
        } else {
          std::cout << "x";
        }
        std::cout << "\t";

        if (ope.data.has_output) { std::cout << output_suffix; }
        std::cout << "\t";

        if (header_.need_state_output) {
          if (ope.data.has_state_output) { std::cout << state_output; }
        }
        std::cout << "\t";

        std::cout << byte_size;
        std::cout << std::endl;
      }

      if (ch == arc) {
        output += output_suffix;
        i++;
        if (ope.data.final) {
          if (prefixes) {
            if (OutputTraits<output_t>::empty(state_output)) {
              prefixes(i, output);
            } else {
              prefixes(i, output + state_output);
            }
            ret = true;
          }
          if (i == len) {
            if (outputs) {
              if (OutputTraits<output_t>::empty(state_output)) {
                outputs(output);
              } else {
                outputs(output + state_output);
              }
            }
            ret = true;
            break;
          }
        }
        if (!next_address) { break; }
        address = next_address;
      } else {
        if (ope.data.last_transition) { break; }
        address -= byte_size;
      }
    }
    longest_prefix_len = i;
    return ret;
  }

  template <typename T, typename U, typename M>
  void
  depth_first_visit_single(uint32_t address, const std::string &partial_word,
                           const output_t &partial_output, const T &transit,
                           U accept, M &accept_mutex,
                           std::string_view prefix = std::string_view()) const {

    const char *jump_table_labels = nullptr;
    size_t jump_table_label_index = 0;

    while (true) {
      auto state_output = output_t{};

      auto end = byte_code_ + address;
      auto p = end;

      auto ope = FstOpe(*p--);

      if (ope.has_jump_table()) {
        auto jump_table_element_size = ope.jump_table_element_size();
        size_t jump_table_count = 0;
        auto vb_len = vb_decode_value_reverse(p, jump_table_count);
        p -= vb_len;
        p -= jump_table_count * jump_table_element_size;

        if (header_.flags.data.jump_table_labels) {
          // The records of this state carry no label bytes; remember the
          // label array and read the labels from it while iterating.
          jump_table_labels = p + 1 - jump_table_count;
          jump_table_label_index = 0;
          p -= jump_table_count;
        }

        address -= std::distance(p, end);
        continue;
      }

      char arc;
      if (jump_table_labels) {
        arc = jump_table_labels[jump_table_label_index++];
      } else {
        arc = read_arc(ope, p);
      }

      uint32_t delta, hub_next_address;
      bool has_hub_next_address;
      read_delta(ope, p, delta, hub_next_address, has_hub_next_address);

      auto output_suffix = output_t{};
      if (ope.data.has_output) {
        p -= OutputTraits<output_t>::read_byte_value(p, output_suffix);
      }

      if (header_.need_state_output) {
        if (ope.data.has_state_output) {
          p -= OutputTraits<output_t>::read_byte_value(p, state_output);
        }
      }

      auto byte_size = std::distance(p, end);

      auto next_address = 0u;
      if (!ope.data.no_address) {
        if (has_hub_next_address) {
          next_address = hub_next_address;
        } else if (delta) {
          next_address = address - byte_size - delta + 1;
        }
      } else {
        next_address = address - byte_size;
      }

      auto atm = transit; // copy
      atm.step(arc);

      auto word = partial_word + arc;
      auto output = partial_output + output_suffix;

      if (ope.data.final) {
        if (atm.is_match()) {
          if (prefix.empty() || (prefix.size() == 1 && prefix.front() == arc)) {
            auto should_append_state_output = false;
            if (OutputTraits<output_t>::type() != OutputType::none_t) {
              if (!OutputTraits<output_t>::empty(state_output)) {
                should_append_state_output = true;
              }
            }

            const auto &final_output =
                should_append_state_output ? output + state_output : output;

            auto acceptor = [&]() {
              // compile-time check: accept supports 3 parameters (word, output,
              // transit)
              if constexpr (std::is_invocable_v<U &, decltype(word),
                                                decltype(final_output),
                                                const T &>) {
                // 3-parameter version: pass atm
                accept(word, final_output, atm);
              } else {
                // 2-parameter version: original logic (compatible with old
                // code)
                accept(word, final_output);
              }
            };

            if constexpr (std::is_same_v<std::decay_t<M>, std::mutex>) {
              std::lock_guard<std::mutex> lock(accept_mutex);
              acceptor();
            } else {
              acceptor();
            }
          }
        }
      }

      if (next_address) {
        if ((prefix.empty() || prefix.front() == arc) && atm.can_match()) {
          depth_first_visit_single(next_address, word, output, atm, accept,
                                   accept_mutex,
                                   prefix.empty() ? prefix : prefix.substr(1));
        }
      }

      if (ope.data.last_transition) { break; }
      address -= byte_size;
    }
  }

  template <typename T, typename U, typename ThreadPool>
  void
  depth_first_visit_parallel(uint32_t address, const std::string &partial_word,
                             const output_t &partial_output, const T &transit,
                             U accept, std::string_view prefix,
                             ThreadPool &thread_pool, std::mutex &accept_mutex,
                             std::vector<std::future<void>> &results) const {

    const char *jump_table_labels = nullptr;
    size_t jump_table_label_index = 0;

    while (true) {
      auto state_output = output_t{};

      auto end = byte_code_ + address;
      auto p = end;

      auto ope = FstOpe(*p--);

      if (ope.has_jump_table()) {
        auto jump_table_element_size = ope.jump_table_element_size();
        size_t jump_table_count = 0;
        auto vb_len = vb_decode_value_reverse(p, jump_table_count);
        p -= vb_len;
        p -= jump_table_count * jump_table_element_size;

        if (header_.flags.data.jump_table_labels) {
          // The records of this state carry no label bytes; remember the
          // label array and read the labels from it while iterating.
          jump_table_labels = p + 1 - jump_table_count;
          jump_table_label_index = 0;
          p -= jump_table_count;
        }

        address -= std::distance(p, end);
        continue;
      }

      char arc;
      if (jump_table_labels) {
        arc = jump_table_labels[jump_table_label_index++];
      } else {
        arc = read_arc(ope, p);
      }

      uint32_t delta, hub_next_address;
      bool has_hub_next_address;
      read_delta(ope, p, delta, hub_next_address, has_hub_next_address);

      auto output_suffix = output_t{};
      if (ope.data.has_output) {
        p -= OutputTraits<output_t>::read_byte_value(p, output_suffix);
      }

      if (header_.need_state_output) {
        if (ope.data.has_state_output) {
          p -= OutputTraits<output_t>::read_byte_value(p, state_output);
        }
      }

      auto byte_size = std::distance(p, end);

      auto next_address = 0u;
      if (!ope.data.no_address) {
        if (has_hub_next_address) {
          next_address = hub_next_address;
        } else if (delta) {
          next_address = address - byte_size - delta + 1;
        }
      } else {
        next_address = address - byte_size;
      }

      auto atm = transit; // copy
      atm.step(arc);

      auto word = partial_word + arc;
      auto output = partial_output + output_suffix;

      if (ope.data.final) {
        if (atm.is_match()) {
          if (prefix.empty() || (prefix.size() == 1 && prefix.front() == arc)) {
            auto should_append_state_output = false;
            if (OutputTraits<output_t>::type() != OutputType::none_t) {
              if (!OutputTraits<output_t>::empty(state_output)) {
                should_append_state_output = true;
              }
            }

            const auto &final_output =
                should_append_state_output ? output + state_output : output;

            std::lock_guard<std::mutex> lock(accept_mutex);
            // compile-time check: accept supports 3 parameters (word,
            // output, transit)
            if constexpr (std::is_invocable_v<U &, decltype(word),
                                              decltype(final_output),
                                              const T &>) {
              // 3-parameter version: pass atm
              accept(word, final_output, atm);
            } else {
              // 2-parameter version: original logic (compatible with old
              // code)
              accept(word, final_output);
            }
          }
        }
      }

      if (next_address) {
        if ((prefix.empty() || prefix.front() == arc) && atm.can_match()) {
          char32_t _;
          if (decode_codepoint(word, _)) {
            results.emplace_back(
                thread_pool.enqueue([this, next_address, word, output, atm,
                                     accept, prefix, &accept_mutex] {
                  depth_first_visit_single(
                      next_address, word, output, atm, accept, accept_mutex,
                      prefix.empty() ? prefix : prefix.substr(1));
                }));
          } else {
            depth_first_visit_parallel(next_address, word, output, atm, accept,
                                       prefix.empty() ? prefix
                                                      : prefix.substr(1),
                                       thread_pool, accept_mutex, results);
          }
        }
      }

      if (ope.data.last_transition) { break; }
      address -= byte_size;
    }
  }

  template <typename T, typename U>
  void depth_first_visit(uint32_t address, const std::string &partial_word,
                         const output_t &partial_output, const T &transit,
                         U accept,
                         std::string_view prefix = std::string_view()) const {
    struct Dummy_mutex {};
    Dummy_mutex dummy_mutex;
    depth_first_visit_single(address, partial_word, partial_output, transit,
                             accept, dummy_mutex, prefix);
  }

  // template <typename T, typename U>
  // void depth_first_visit(uint32_t address, const std::string
  // &partial_word,
  //                        const output_t &partial_output, const T
  //                        &transit, U accept, std::string_view prefix =
  //                        std::string_view()) const
  //                        {
  //   std::mutex accept_mutex;
  //   std::vector<std::future<void>> results;
  //   ThreadPool thread_pool(8);
  //   depth_first_visit_parallel(address, partial_word, partial_output,
  //   transit,
  //                              accept, prefix, thread_pool, accept_mutex,
  //                              results);

  //   for (auto &result : results) {
  //     result.get();
  //   }
  // }

  template <typename T, typename U, typename ThreadPool>
  void depth_first_visit(uint32_t address, const std::string &partial_word,
                         const output_t &partial_output, const T &transit,
                         U accept, ThreadPool &thread_pool,
                         std::string_view prefix = std::string_view()) const {
    std::mutex accept_mutex;
    std::vector<std::future<void>> results;
    depth_first_visit_parallel(address, partial_word, partial_output, transit,
                               accept, prefix, thread_pool, accept_mutex,
                               results);

    for (auto &result : results) {
      result.get();
    }
  }

  char read_arc(FstOpe ope, const char *&p) const {
    auto index =
        ope.label_index(header_.need_output, header_.need_state_output);
    return index == 0 ? *p-- : header_.char_index[index];
  }

  void read_delta(FstOpe ope, const char *&p, uint32_t &delta,
                  uint32_t &hub_next_address,
                  bool &has_hub_next_address) const {
    delta = 0;
    hub_next_address = 0;
    has_hub_next_address = false;
    if (!ope.data.no_address) {
      p -= vb_decode_value_reverse(p, delta);
      if (header_.flags.data.hub_table) {
        if (delta & 1) {
          // Odd values are hub table indexes.
          hub_next_address = header_.hub_address(delta >> 1);
          has_hub_next_address = true;
          delta = 0;
        } else {
          delta >>= 1;
        }
      }
    }
  }

  size_t lookup_jump_table(const char *p, size_t index,
                           size_t element_size) const {
    if (element_size == 2) {
      return reinterpret_cast<const uint16_t *>(p + 1)[index];
    } else {
      return reinterpret_cast<const uint8_t *>(p + 1)[index];
    }
  }

  const char *byte_code_;
  const size_t byte_code_size_;

  FstHeader header_;
  bool is_valid_ = false;
  bool trace_ = false;

  // Suggestion
  template <typename T>
  decltype(auto) suggest_core(std::string_view word, const T &matcher) const {
    std::vector<std::unique_ptr<std::pair<double, std::string>>> suggestions;
    size_t c_len = calc_c_len(word);
    if (c_len == 0) { return suggestions; }
    size_t min_edits = c_len <= 1 ? 1 : 2;
    size_t max_edits = c_len;

    for (size_t edits = min_edits; edits <= max_edits; edits++) {
      auto results = matcher.edit_distance_search(word, edits, 1, 1, 2);

      if (results.size() >= 1) {
        for (auto &result : results) {
          std::string &candidate = *result;
          if (candidate == word) {
            suggestions.emplace_back(
                std::make_unique<std::pair<double, std::string>>(
                    1, std::move(candidate)));
          } else {
            auto jw = jaro_winkler_distance(word, candidate);
            auto le = levenshtein_distance(word, candidate);
            auto similarity = jw * le;
            suggestions.emplace_back(
                std::make_unique<std::pair<double, std::string>>(
                    similarity, std::move(candidate)));
          }
        }

        if (!suggestions.empty()) { break; }
      }
    }
    return suggestions;
  }
};

} // namespace fst