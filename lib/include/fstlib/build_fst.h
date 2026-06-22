//
//  fstlib.h
//
//  Copyright (c) 2022 Yuji Hirose. All rights reserved.
//  MIT License
//

#pragma once
#include <numeric>
#include <cassert>

#include <fstlib/output_traits.h>
#include <fstlib/minimize.h>

namespace fst {

//-----------------------------------------------------------------------------
// get_common_prefix_and_word_suffix
//-----------------------------------------------------------------------------

template <typename output_t>
inline void get_common_prefix_and_word_suffix(const output_t &current_output,
                                              const output_t &output,
                                              output_t &common_prefix,
                                              output_t &word_suffix) {
  common_prefix =
      OutputTraits<output_t>::get_common_prefix(output, current_output);
  word_suffix = OutputTraits<output_t>::get_suffix(output, common_prefix);
}

//-----------------------------------------------------------------------------
// build_fst_core
//-----------------------------------------------------------------------------

enum class Result { Success, EmptyKey, UnsortedKey, DuplicateKey };

template <typename output_t, typename Input, typename Writer>
inline std::pair<Result, size_t>
build_fst_core(const Input &input, Writer &writer, bool need_output,
               bool keep_all_states = false) {
  StatePool<output_t> state_pool;

  Dictionary<output_t> dictionary(state_pool, keep_all_states);
  auto next_state_id = 0u;
  auto error_input_index = 0u;
  auto result = Result::Success;

  // Main algorithm ported from the technical paper
  std::vector<State<output_t> *> temp_states;
  std::string previous_word;
  temp_states.push_back(state_pool.New(next_state_id++));

  input([&](const auto &current_word, const auto &_current_output,
            size_t input_index) {
    auto current_output = _current_output;

    if (current_word.empty()) {
      result = Result::EmptyKey;
      error_input_index = input_index;
      return false;
    }

    // The following loop caluculates the length of the longest common
    // prefix of 'current_word' and 'previous_word'
    size_t prefix_length;
    if (!get_prefix_length(previous_word, current_word, prefix_length)) {
      result = Result::UnsortedKey;
      error_input_index = input_index;
      return false;
    }

    if (previous_word.size() == current_word.size() &&
        previous_word == current_word) {
      result = Result::DuplicateKey;
      error_input_index = input_index;
      return false;
    }

    // We minimize the states from the suffix of the previous word
    for (auto i = previous_word.size(); i > prefix_length; i--) {
      auto [found, state] =
          find_minimized<output_t>(temp_states[i], dictionary);

      auto arc = previous_word[i - 1];

      if (found) {
        next_state_id--;
      } else {
        writer.write(*state, arc);

        // Ownership of the object in temp_states[i] has been moved to the
        // dictionary...
        temp_states[i] = state_pool.New();
      }

      temp_states[i - 1]->update_last_transition(state);
    }

    // This loop initializes the tail states for the current word
    for (auto i = prefix_length + 1; i <= current_word.size(); i++) {
      assert(i <= temp_states.size());
      if (i == temp_states.size()) {
        temp_states.push_back(state_pool.New(next_state_id++));
      } else {
        temp_states[i]->reuse(next_state_id++);
      }
      auto arc = current_word[i - 1];
      temp_states[i - 1]->add_transition(arc, temp_states[i]);
    }

    if (current_word != previous_word) {
      auto state = temp_states[current_word.size()];
      state->set_final(true);
    }

    if (need_output) {
      for (auto j = 1u; j <= prefix_length; j++) {
        auto prev_state = temp_states[j - 1];
        auto arc = current_word[j - 1];

        const auto &output = prev_state->output(arc);

        auto common_prefix = output_t{};
        auto word_suffix = output_t{};
        get_common_prefix_and_word_suffix(current_output, output, common_prefix,
                                          word_suffix);

        prev_state->set_output(arc, common_prefix);

        if (!OutputTraits<output_t>::empty(word_suffix)) {
          auto state = temp_states[j];

          for (auto arc : state->transitions.arcs) {
            state->prepend_suffix_to_output(arc, word_suffix);
          }

          if (state->final) {
            state->prepend_suffix_to_state_outputs(word_suffix);
          }
        }

        current_output =
            OutputTraits<output_t>::get_suffix(current_output, common_prefix);
      }

      if (current_word == previous_word) {
        auto state = temp_states[current_word.size()];
        state->push_to_state_outputs(current_output);
      } else {
        auto state = temp_states[prefix_length];
        auto arc = current_word[prefix_length];
        state->set_output(arc, current_output);
      }
    }

    previous_word = current_word;
    return true;
  });

  if (result != Result::Success) {
    return std::pair(result, error_input_index);
  }

  // Here we are minimizing the states of the last word
  State<output_t> *root = nullptr;
  for (auto i = static_cast<int>(previous_word.size()); i >= 0; i--) {
    auto [found, state] = find_minimized<output_t>(temp_states[i], dictionary);

    auto arc = (i > 0) ? previous_word[i - 1] : 0;

    if (found) {
      next_state_id--;
    } else {
      writer.write(*state, arc);
    }

    if (i > 0) {
      temp_states[i - 1]->update_last_transition(state);
    } else {
      root = state;
    }
  }

  writer.finish(*root);

  return std::pair(Result::Success, error_input_index);
}

//-----------------------------------------------------------------------------
// build_fst
//-----------------------------------------------------------------------------

template <typename output_t, typename Input, typename Writer>
inline std::pair<Result, size_t>
build_fst(const Input &input, Writer &writer, bool need_output, bool sorted,
          bool keep_all_states = false,
          std::function<void(size_t)> progress = nullptr) {
  return build_fst_core<output_t>(
      [&](const auto &feeder) {
        if (sorted) {
          size_t input_index = 0;
          for (const auto &item : input) {
            const auto &word = item.first;
            const auto &output = item.second;
            if (!feeder(word, output, input_index)) { break; }
            if (progress) { progress(input_index); }
            input_index++;
          }
        } else {
          std::vector<size_t> sorted_indexes(input.size());
          {
            std::iota(sorted_indexes.begin(), sorted_indexes.end(), 0);
            std::sort(sorted_indexes.begin(), sorted_indexes.end(),
                      [&](const auto &a, const auto &b) {
                        return input[a].first < input[b].first;
                      });
          }
          size_t index = 0;
          for (auto input_index : sorted_indexes) {
            const auto &[word, output] = input[input_index];
            if (!feeder(word, output, input_index)) { break; }
            if (progress) { progress(index); }
            index += 1;
          }
        }
      },
      writer, need_output, keep_all_states);
}

template <typename Input, typename Writer>
inline std::pair<Result, size_t> build_fst(const Input &input, Writer &writer,
                                           bool need_output, bool sorted,
                                           bool keep_all_states = false) {
  return build_fst_core<uint32_t>(
      [&](const auto &feeder) {
        if (sorted) {
          size_t input_index = 0;
          for (const auto &word : input) {
            if (!feeder(word, static_cast<uint32_t>(input_index),
                        input_index)) {
              break;
            }
            input_index++;
          }
        } else {
          std::vector<size_t> sorted_indexes(input.size());
          {
            std::iota(sorted_indexes.begin(), sorted_indexes.end(), 0);
            std::sort(sorted_indexes.begin(), sorted_indexes.end(),
                      [&](const auto &a, const auto &b) {
                        return input[a] < input[b];
                      });
          }

          for (auto input_index : sorted_indexes) {
            const auto word = input[input_index];
            if (!feeder(word, static_cast<uint32_t>(input_index),
                        input_index)) {
              break;
            }
          }
        }
      },
      writer, need_output, keep_all_states);
}

} // namespace fst