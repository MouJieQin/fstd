//
//  fstlib.h
//
//  Copyright (c) 2022 Yuji Hirose. All rights reserved.
//  MIT License
//

#pragma once

#include <fstlib/output_traits.h>
#include <fstlib/byte_code.h>

namespace fst {

//-----------------------------------------------------------------------------
// State
//-----------------------------------------------------------------------------

template <typename output_t> class State {
public:
  struct Transition {
    size_t id;
    bool final;
    output_t state_output;
    output_t output;

    bool operator==(const Transition &rhs) const {
      if (this != &rhs) {
        return id == rhs.id && final == rhs.final &&
               state_output == rhs.state_output && output == rhs.output;
      }
      return true;
    }
  };

  class Transitions {
  public:
    std::vector<char> arcs;
    std::vector<Transition> states_and_outputs;

    bool operator==(const Transitions &rhs) const {
      if (this != &rhs) {
        return arcs == rhs.arcs && states_and_outputs == rhs.states_and_outputs;
      }
      return true;
    }

    size_t size() const { return arcs.size(); }

    bool empty() const { return !size(); }

    const output_t &output(char arc) const {
      auto idx = get_index(arc);
      assert(idx != -1);
      return states_and_outputs[idx].output;
    }

    template <typename Functor> void for_each(Functor fn) const {
      for (auto i = 0u; i < arcs.size(); i++) {
        fn(arcs[i], states_and_outputs[i]);
      }
    }

  private:
    void clear() {
      arcs.clear();
      states_and_outputs.clear();
    }

    // The minimization loop always rewires the most recently added arc.
    void update_last_transition(State<output_t> *state) {
      auto &t = states_and_outputs.back();
      t.id = state->id;
      t.final = state->final;
      t.state_output = state->state_output;
    }

    // The tail initialization always adds a new arc.
    void add_transition(char arc, State<output_t> *state) {
      arcs.push_back(arc);
      auto &t = states_and_outputs.emplace_back(Transition());
      t.id = state->id;
      t.final = state->final;
      t.state_output = state->state_output;
    }

    void set_transition(char arc, State<output_t> *state) {
      auto idx = get_index(arc);
      if (idx == -1) {
        idx = static_cast<int>(arcs.size());
        arcs.push_back(arc);
        states_and_outputs.emplace_back(Transition());
      }
      states_and_outputs[idx].id = state->id;
      states_and_outputs[idx].final = state->final;
      states_and_outputs[idx].state_output = state->state_output;
    }

    void set_output(char arc, const output_t &val) {
      auto idx = get_index(arc);
      states_and_outputs[idx].output = val;
    }

    void insert_output(char arc, const output_t &val) {
      auto idx = get_index(arc);
      auto &output = states_and_outputs[idx].output;
      OutputTraits<output_t>::prepend_value(output, val);
    }

    int get_index(char arc) const {
      for (auto i = 0u; i < arcs.size(); i++) {
        if (arcs[i] == arc) { return static_cast<int>(i); }
      }
      return -1;
    }

    friend class State;
  };

  State(size_t id) : id(id) {}

  const output_t &output(char arc) const { return transitions.output(arc); }

  bool operator==(const State &rhs) const {
    if (this != &rhs) {
      return final == rhs.final && transitions == rhs.transitions &&
             state_output == rhs.state_output;
    }
    return true;
  }

  uint64_t hash() const;

  void set_final(bool final) { this->final = final; }

  void set_transition(char arc, State<output_t> *state) {
    transitions.set_transition(arc, state);
  }

  void update_last_transition(State<output_t> *state) {
    transitions.update_last_transition(state);
  }

  void add_transition(char arc, State<output_t> *state) {
    transitions.add_transition(arc, state);
  }

  void set_output(char arc, const output_t &output) {
    transitions.set_output(arc, output);
  }

  void prepend_suffix_to_output(char arc, const output_t &suffix) {
    transitions.insert_output(arc, suffix);
  }

  void push_to_state_outputs(const output_t &output) { state_output = output; }

  void prepend_suffix_to_state_outputs(const output_t &suffix) {
    OutputTraits<output_t>::prepend_value(state_output, suffix);
  }

  void reuse(size_t state_id) {
    id = state_id;
    set_final(false);
    transitions.clear();
    state_output = output_t{};
  }

  size_t id = -1;
  bool final = false;
  Transitions transitions;
  output_t state_output = output_t{};

private:
  State(const State &) = delete;
  State(State &&) = delete;
};

template <typename output_t> inline uint64_t State<output_t>::hash() const {
  auto h = kFnvBasis;

  transitions.for_each([&](char arc, const State::Transition &t) {
    hash_bytes(h, &arc, sizeof(arc));

    auto val = static_cast<uint32_t>(t.id);
    hash_bytes(h, &val, sizeof(val));

    if (!OutputTraits<output_t>::empty(t.output)) {
      OutputTraits<output_t>::hash_value(h, t.output);
    }
  });

  if (final && !OutputTraits<output_t>::empty(state_output)) {
    OutputTraits<output_t>::hash_value(h, state_output);
  }

  // Final mixing improves the bucket distribution in the dictionary.
  h ^= h >> 33;
  h *= 0xff51afd7ed558ccdULL;
  h ^= h >> 33;
  return h;
}

} // namespace fst