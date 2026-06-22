//
//  fstlib.h
//
//  Copyright (c) 2022 Yuji Hirose. All rights reserved.
//  MIT License
//

#pragma once
#include <iostream>

#include <fstlib/state_transition.h>

namespace fst {

template <typename output_t, bool need_state_output> class FstWriter {
public:
  // With 'single_pass' (which requires building with 'keep_all_states'),
  // the writer collects the states during the build and emits all records
  // in finish(), where the most referenced states (hubs) are known and
  // placed in an address table, so that references to them are encoded as
  // short table indexes instead of long deltas.
  template <typename Input>
  FstWriter(std::ostream &os, bool need_output, bool dump, bool verbose,
            const Input &input, bool single_pass = false)
      : os_(os), need_output_(need_output), dump_(dump), verbose_(verbose),
        single_pass_(single_pass && !dump) {

    initialize_char_index_table(input);

    if (dump_) {
      os << "Address\tArc\tN F L\tNxtAddr";
      if (need_output_) { os << "\tOutput\tStOuts"; }
      os << "\tSize" << std::endl;

      os << "-------\t---\t-----\t-------";
      if (need_output_) { os << "\t------\t------"; }
      os << "\t----" << std::endl;
    }
  }

  ~FstWriter() {
    if (address_table_.empty()) { return; }

    auto start_byte_adress = address_table_.back();

    auto output_type =
        need_output_ ? OutputTraits<output_t>::type() : OutputType::none_t;

    if (!dump_) {
      for (auto id : hub_ids_) {
        auto address =
            static_cast<uint32_t>(address_table_[record_index_map_[id]]);
        os_.write(reinterpret_cast<char *>(&address), sizeof(address));
      }
    }

    FstHeader header(output_type, need_state_output, start_byte_adress,
                     char_index_table_, hub_ids_.size());

    if (!dump_) { header.write(os_); }

    if (verbose_) {
      const size_t char_index_size =
          FstOpe::char_index_size(need_output_, need_state_output);
      const size_t hub_table_size =
          hub_ids_.empty() ? 0 : (hub_ids_.size() + 1) * sizeof(uint32_t);
      const size_t total_size = address_ + hub_table_size + char_index_size +
                                sizeof(uint32_t) + sizeof(uint8_t);
      const auto unique_char_count =
          std::count_if(std::begin(char_count_), std::end(char_count_),
                        [](auto count) { return count > 0; });
      std::cerr << "# unique char count: " << unique_char_count << std::endl;
      std::cerr << "# state count: " << written_state_count_ << std::endl;
      std::cerr << "# record count: " << address_table_.size() << std::endl;
      std::cerr << "# total size: " << total_size << std::endl;
    }
  }

  void write(const State<output_t> &state, char prev_arc) {
    if (single_pass_) {
      // Just collect the state; the records are emitted in finish() once
      // the reference counts are known. The state object stays alive
      // because the build keeps all states.
      if (state.id >= states_by_id_.size()) {
        states_by_id_.resize(state.id + state.id / 2 + 16, nullptr);
      }
      states_by_id_[state.id] = &state;
      return;
    }

    write_state_records(state, prev_arc);
  }

  void finish(const State<output_t> &root) {
    if (!single_pass_) { return; }

    initialize_hub_ranks();

    // Write the records by a post-order traversal, which reproduces the
    // order of the incremental write: a target state is always written
    // before the states that reference it.
    struct Frame {
      const State<output_t> *state;
      size_t transition_index;
      char arc;
    };

    std::vector<bool> visited(states_by_id_.size(), false);
    std::vector<Frame> stack;

    visited[root.id] = true;
    stack.push_back({&root, 0, 0});

    while (!stack.empty()) {
      auto &frame = stack.back();
      if (frame.transition_index < frame.state->transitions.size()) {
        auto i = frame.transition_index++;
        const auto &t = frame.state->transitions.states_and_outputs[i];
        auto child = states_by_id_[t.id];
        if (child && !visited[t.id] && !child->transitions.empty()) {
          visited[t.id] = true;
          stack.push_back({child, 0, frame.state->transitions.arcs[i]});
        }
      } else {
        write_state_records(*frame.state, frame.arc);
        stack.pop_back();
      }
    }
  }

  void write_state_records(const State<output_t> &state, char prev_arc) {
    auto transition_count = state.transitions.size();
    const auto &[arcs, states_and_outputs] = state.transitions;

    auto char_index_size =
        FstOpe::char_index_size(need_output_, need_state_output);

    std::vector<size_t> jump_table(transition_count);
    auto need_jump_table = transition_count >= 8;

    // Only states with a jump table carry a label array; avoid the
    // allocation for the small states that make up the majority.
    std::vector<char> jump_table_labels;
    if (need_jump_table) { jump_table_labels.resize(transition_count); }

    size_t indexes_sorted_by_bigram_count[256];

    if (!need_jump_table) {
      uint16_t keys[256];
      for (auto i = 0u; i < arcs.size(); i++) {
        indexes_sorted_by_bigram_count[i] = i;
        keys[i] = bigram_key(prev_arc, arcs[i]);
      }

      std::sort(&indexes_sorted_by_bigram_count[0],
                &indexes_sorted_by_bigram_count[arcs.size()],
                [&](auto i1, auto i2) {
                  return bigram_count_[keys[i1]] > bigram_count_[keys[i2]];
                });
    }

    for (auto ri = arcs.size(); ri > 0; ri--) {
      auto i = ri - 1;

      auto arc_i = !need_jump_table ? indexes_sorted_by_bigram_count[i] : i;
      auto arc = arcs[arc_i];
      const auto &t = states_and_outputs[arc_i];

      auto record_index = record_index_of(t.id);
      auto has_address = record_index >= 0;
      auto last_transition = transition_count - 1 == i;
      auto no_address =
          last_transition && has_address &&
          record_index == static_cast<int64_t>(address_table_.size() - 1);

      // If the state has 6 or more transitions, then generate jump table.
      auto generate_jump_table = (i == 0) && need_jump_table;

      FstRecord<output_t, need_state_output> rec;
      rec.need_output = need_output_;
      rec.ope.data.no_address = no_address;
      rec.ope.data.last_transition = last_transition;
      rec.ope.data.final = t.final;

      rec.delta = 0;
      auto next_address = 0u;
      if (!no_address) {
        if (has_address) {
          auto delta = address_ - address_table_[record_index];
          next_address = address_ - delta;
          rec.delta = delta;

          if (!hub_ids_.empty()) {
            // Real deltas are doubled; references to hub states are encoded
            // as (rank * 2 + 1) when that is not longer than the delta.
            rec.delta = delta * 2;
            auto hub_rank = hub_rank_of(t.id);
            if (hub_rank >= 0) {
              auto index_value = static_cast<size_t>(hub_rank) * 2 + 1;
              if (vb_encode_value_length(index_value) <=
                  vb_encode_value_length(rec.delta)) {
                rec.delta = index_value;
              }
            }
          }
        }
      }

      if (need_output_) {
        rec.ope.data.has_output = false;
        if (!OutputTraits<output_t>::empty(t.output)) {
          rec.ope.data.has_output = true;
          rec.output = &t.output;
        }

        if (need_state_output) {
          rec.ope.data.has_state_output = false;
          if (!OutputTraits<output_t>::empty(t.state_output)) {
            rec.ope.data.has_state_output = true;
            rec.state_output = &t.state_output;
          }
        }
      }

      if (need_jump_table) {
        // The label is stored in the state's jump table instead of the
        // record itself. label_index 0 also keeps the ope byte away from
        // the jump table tag values (0xff/0xfe).
        rec.omit_label = true;
        rec.ope.set_label_index(need_output_, need_state_output, 0);
        jump_table_labels[i] = arc;
      } else {
        auto label_index = 0u;
        auto index = char_index_table_[static_cast<uint8_t>(arc)];
        if (index < char_index_size) {
          label_index = index;
        } else {
          rec.label = arc;
        }
        rec.ope.set_label_index(need_output_, need_state_output, label_index);

        // When the ope byte happens to be the same as jump tag byte, change to
        // use '.label' field instead.
        if (rec.ope.has_jump_table()) {
          rec.label = arc;
          rec.ope.set_label_index(need_output_, need_state_output, 0);
        }
      }

      auto byte_size = rec.byte_size();
      auto accessible_address = address_ + byte_size - 1;
      address_table_.push_back(accessible_address);
      address_ += byte_size;

      if (!dump_) {
        rec.write(os_);

        if (need_jump_table) { jump_table[i] = accessible_address; }

        if (generate_jump_table) {
          auto jump_table_element_size = 1;

          for (auto &val : jump_table) {
            val = accessible_address - val;
            if (val > 0xff) { jump_table_element_size = 2; }
          }

          auto jump_table_byte_size =
              1 + vb_encode_value_length(jump_table.size()) +
              jump_table.size() * jump_table_element_size +
              jump_table_labels.size();

          auto need_two_bytes = jump_table_element_size == 2;

          auto jump_table_tag = FstOpe::jump_table_tag(need_two_bytes);

          byte_size += jump_table_byte_size;
          address_table_[address_table_.size() - 1] += jump_table_byte_size;
          address_ += jump_table_byte_size;

          os_.write(jump_table_labels.data(), jump_table_labels.size());

          if (need_two_bytes) {
            write_jump_table<uint16_t>(jump_table);
          } else {
            write_jump_table<uint8_t>(jump_table);
          }

          vb_encode_value_reverse(jump_table.size(), os_);

          os_.write((char *)&jump_table_tag, 1);
        }
      } else {
        os_ << address_table_.back() << "\t";
        os_ << char_to_string(arc) << "\t";

        os_ << (no_address ? "↑" : " ") << ' ' << (t.final ? '*' : ' ') << ' '
            << (last_transition ? "‾" : " ") << "\t";

        if (!no_address) {
          if (next_address > 0) {
            os_ << next_address;
          } else {
            os_ << "x";
          }
        }

        if (need_output_) {
          os_ << "\t";

          if (!OutputTraits<output_t>::empty(t.output)) { os_ << t.output; }
          os_ << "\t";

          if (!OutputTraits<output_t>::empty(t.state_output)) {
            os_ << t.state_output;
          }
        }

        os_ << "\t" << byte_size << std::endl;
      }
    }

    if (!state.transitions.empty()) {
      if (state.id >= record_index_map_.size()) {
        record_index_map_.resize(state.id + state.id / 2 + 16, -1);
      }
      record_index_map_[state.id] =
          static_cast<int64_t>(address_table_.size() - 1);
      written_state_count_++;
    }
  }

private:
  template <typename Input>
  void initialize_char_index_table(const Input &input) {
    char_index_table_.assign(256, 0);

    input([&](const auto &word) {
      char prev = 0;
      for (auto ch : word) {
        char_count_[static_cast<uint8_t>(ch)]++;
        bigram_count_[bigram_key(prev, ch)]++;
        prev = ch;
      }
    });

    struct second_order {
      bool operator()(const std::pair<char, size_t> &x,
                      const std::pair<char, size_t> &y) const {
        return x.second < y.second;
      }
    };

    std::priority_queue<std::pair<char, size_t>,
                        std::vector<std::pair<char, size_t>>, second_order>
        que;

    for (auto ch = 0u; ch < 256; ch++) {
      if (char_count_[ch] > 0) {
        que.push(std::pair(static_cast<char>(ch), char_count_[ch]));
      }
    }

    auto index = 1u;
    while (!que.empty()) {
      auto [ch, count] = que.top();
      char_index_table_[static_cast<uint8_t>(ch)] = index++;
      que.pop();
    }
  }

  template <typename T>
  void write_jump_table(const std::vector<size_t> &jump_table) {
    std::vector<T> table(jump_table.size());
    for (auto i = 0u; i < jump_table.size(); i++) {
      table[i] = static_cast<T>(jump_table[i]);
    }
    os_.write((char *)table.data(), table.size() * sizeof(T));
  }

  void initialize_hub_ranks() {
    std::vector<uint32_t> ref_counts(states_by_id_.size(), 0);
    for (auto state : states_by_id_) {
      if (!state) { continue; }
      for (const auto &t : state->transitions.states_and_outputs) {
        ref_counts[t.id]++;
      }
    }

    std::vector<std::pair<size_t, size_t>> candidates; // (count, id)
    for (auto id = 0u; id < states_by_id_.size(); id++) {
      if (ref_counts[id] >= 2 && states_by_id_[id] &&
          !states_by_id_[id]->transitions.empty()) {
        candidates.emplace_back(ref_counts[id], id);
      }
    }

    std::sort(
        candidates.begin(), candidates.end(), [](const auto &a, const auto &b) {
          return a.first == b.first ? a.second < b.second : a.first > b.first;
        });

    if (candidates.size() > kMaxHubCount) { candidates.resize(kMaxHubCount); }

    hub_rank_by_id_.assign(states_by_id_.size(), -1);
    hub_ids_.reserve(candidates.size());
    for (auto i = 0u; i < candidates.size(); i++) {
      hub_rank_by_id_[candidates[i].second] = static_cast<int32_t>(i);
      hub_ids_.push_back(candidates[i].second);
    }
  }

  int64_t record_index_of(size_t id) const {
    return id < record_index_map_.size() ? record_index_map_[id] : -1;
  }

  int32_t hub_rank_of(size_t id) const {
    return id < hub_rank_by_id_.size() ? hub_rank_by_id_[id] : -1;
  }

  std::ostream &os_;
  size_t need_output_ = true;
  size_t dump_ = true;
  size_t verbose_ = true;

  size_t char_count_[256] = {0};
  std::vector<size_t> char_index_table_;

  uint16_t bigram_key(char prev, char cur) const {
    return static_cast<uint16_t>(prev) << 8 | static_cast<uint16_t>(cur);
  }
  std::vector<size_t> bigram_count_ = std::vector<size_t>(65536, 0);

  std::vector<int64_t> record_index_map_; // by state id, -1 = absent
  size_t written_state_count_ = 0;

  size_t address_ = 0;
  std::vector<size_t> address_table_;

  static constexpr size_t kMaxHubCount = 1024;
  std::vector<int32_t> hub_rank_by_id_; // by state id, -1 = not a hub
  std::vector<size_t> hub_ids_;

  bool single_pass_ = false;
  std::vector<const State<output_t> *> states_by_id_;
};

//-----------------------------------------------------------------------------
// dot
//-----------------------------------------------------------------------------

template <typename output_t> class DotWriter {
public:
  DotWriter(std::ostream &os) : os_(os) {
    os_ << "digraph{" << std::endl;
    os_ << "  rankdir = LR;" << std::endl;
  }

  ~DotWriter() { os_ << "}" << std::endl; }

  void write(const State<output_t> &state, char _) {
    if (state.final) {
      auto state_output = OutputTraits<output_t>::init_value();
      if (!OutputTraits<output_t>::empty(state.state_output)) {
        state_output = state.state_output;
      }
      os_ << "  s" << state.id << " [ shape = doublecircle, xlabel = \""
          << state_output << "\" ];" << std::endl;
    } else {
      os_ << "  s" << state.id << " [ shape = circle ];" << std::endl;
    }

    state.transitions.for_each(
        [&](auto arc, const typename State<output_t>::Transition &t) {
          std::string label;
          label += arc;
          os_ << "  s" << state.id << "->s" << t.id << " [ label = \"" << label;
          if (!OutputTraits<output_t>::empty(t.output)) {
            os_ << " (" << t.output << ")";
          }
          os_ << "\" fontcolor = red ];" << std::endl;
        });
  }

  void finish(const State<output_t> &_) {}

private:
  std::ostream &os_;
};

} // namespace fst