//
//  fstlib.h
//
//  Copyright (c) 2022 Yuji Hirose. All rights reserved.
//  MIT License
//

#pragma once
#include <fstlib/byte_code.h>
#include <fstlib/state_transition.h>

namespace fst {

//-----------------------------------------------------------------------------
// StatePool
//-----------------------------------------------------------------------------

template <typename output_t> class StatePool {
public:
  ~StatePool() {
    for (auto p : object_pool_) {
      delete p;
    }
  }

  State<output_t> *New(size_t state_id = -1) {
    if (!free_list_.empty()) {
      auto p = free_list_.back();
      free_list_.pop_back();
      p->reuse(state_id);
      return p;
    }
    auto p = new State<output_t>(state_id);
    object_pool_.push_back(p);
    return p;
  }

  // Recycled states keep their transition vector capacities, which saves
  // a large number of allocations during the build.
  void Delete(State<output_t> *p) { free_list_.push_back(p); }

private:
  std::vector<State<output_t> *> object_pool_;
  std::vector<State<output_t> *> free_list_;
};

//-----------------------------------------------------------------------------
// Dictionary
//-----------------------------------------------------------------------------

template <typename output_t> class Dictionary {
public:
  // With 'keep_all', the dictionary keeps every minimized state in a
  // growable open addressing table, so no duplicate states are created.
  // Otherwise it works as a fixed size 3-way LRU cache, which bounds the
  // memory usage but produces duplicate states on eviction.
  Dictionary(StatePool<output_t> &state_pool, bool keep_all)
      : state_pool_(state_pool), keep_all_(keep_all) {
    if (keep_all_) {
      table_.resize(kInitialTableSize, {0, nullptr});
    } else {
      buckets_.resize(kBucketCount, {{0, nullptr}, {0, nullptr}, {0, nullptr}});
    }
  }

  State<output_t> *get(uint64_t key, State<output_t> *state) {
    if (keep_all_) {
      auto mask = table_.size() - 1;
      auto i = key & mask;
      while (table_[i].second) {
        // Compare the hash keys first to avoid expensive state comparisons.
        if (table_[i].first == key && *table_[i].second == *state) {
          return table_[i].second;
        }
        i = (i + 1) & mask;
      }
      return nullptr;
    }

    auto id = bucket_id(key);
    auto [first, second, third] = buckets_[id];
    // Compare the hash keys first to avoid expensive state comparisons.
    if (first.second && first.first == key && *first.second == *state) {
      return first.second;
    }
    if (second.second && second.first == key && *second.second == *state) {
      buckets_[id] = std::tuple(second, first, third);
      return second.second;
    }
    if (third.second && third.first == key && *third.second == *state) {
      buckets_[id] = std::tuple(third, first, second);
      return third.second;
    }
    return nullptr;
  }

  void put(uint64_t key, State<output_t> *state) {
    if (keep_all_) {
      if (count_ * 10 >= table_.size() * 7) { grow_table(); }
      auto mask = table_.size() - 1;
      auto i = key & mask;
      while (table_[i].second) {
        i = (i + 1) & mask;
      }
      table_[i] = {key, state};
      count_++;
      return;
    }

    auto id = bucket_id(key);
    auto [first, second, third] = buckets_[id];
    if (third.second) { state_pool_.Delete(third.second); }
    buckets_[id] = std::tuple(Entry{key, state}, first, second);
  }

private:
  StatePool<output_t> &state_pool_;
  bool keep_all_;

  static const auto kBucketCount = 10000u;
  static const auto kInitialTableSize = 1u << 16;

  size_t bucket_id(uint64_t key) const { return key % kBucketCount; }

  using Entry = std::pair<uint64_t, State<output_t> *>;

  void grow_table() {
    std::vector<Entry> old_table(table_.size() * 2, {0, nullptr});
    table_.swap(old_table);
    auto mask = table_.size() - 1;
    for (const auto &entry : old_table) {
      if (!entry.second) { continue; }
      auto i = entry.first & mask;
      while (table_[i].second) {
        i = (i + 1) & mask;
      }
      table_[i] = entry;
    }
  }

  std::vector<std::tuple<Entry, Entry, Entry>> buckets_;
  std::vector<Entry> table_;
  size_t count_ = 0;
};

//-----------------------------------------------------------------------------
// find_minimized
//-----------------------------------------------------------------------------

template <typename output_t>
inline std::pair<bool, State<output_t> *>
find_minimized(State<output_t> *state, Dictionary<output_t> &dictionary) {
  auto h = state->hash();

  auto st = dictionary.get(h, state);
  if (st) { return std::pair(true, st); }

  dictionary.put(h, state);
  return std::pair(false, state);
};

} // namespace fst