//
//  fstlib.h
//
//  Copyright (c) 2022 Yuji Hirose. All rights reserved.
//  MIT License
//

#pragma once

#include <fstlib/output_traits.h>
#include <fstlib/utility.h>

namespace fst {

//-----------------------------------------------------------------------------
// compile
//-----------------------------------------------------------------------------

union FstOpe {
  struct {
    unsigned no_address : 1;
    unsigned last_transition : 1;
    unsigned final : 1;
    unsigned has_output : 1;
    unsigned has_state_output : 1;
    unsigned label_index : 3;
  } data;

  struct {
    unsigned no_address : 1;
    unsigned last_transition : 1;
    unsigned final : 1;
    unsigned has_output : 1;
    unsigned label_index : 4;
  } data_no_state_output;

  struct {
    unsigned no_address : 1;
    unsigned last_transition : 1;
    unsigned final : 1;
    unsigned label_index : 5;
  } data_no_output;

  uint8_t byte = 0;

  FstOpe() = default;
  explicit FstOpe(uint8_t byte) : byte(byte) {}

  size_t label_index(bool need_output, bool need_state_output) const {
    if (!need_output) {
      return data_no_output.label_index;
    } else if (need_state_output) {
      return data.label_index;
    } else {
      return data_no_state_output.label_index;
    }
  }

  void set_label_index(bool need_output, bool need_state_output, size_t index) {
    if (!need_output) {
      data_no_output.label_index = index;
    } else if (need_state_output) {
      data.label_index = index;
    } else {
      data_no_state_output.label_index = index;
    }
  }

  // For char index
  static constexpr size_t char_index_size(bool need_output,
                                          bool need_state_output) {
    return !need_output ? 32 : (need_state_output ? 8 : 16);
  }

  // For jump table
  bool has_jump_table() const { return byte == 0xff || byte == 0xfe; }

  size_t jump_table_element_size() const {
    return (byte == 0xff || byte == 0xfd) ? 2 : 1;
  }

  static uint8_t jump_table_tag(bool need_two_bytes) {
    return need_two_bytes ? 0xff : 0xfe;
  }
};

template <typename output_t, bool need_state_output> struct FstRecord {
  FstOpe ope;

  char label = 0;
  size_t delta = 0;
  bool need_output = false;
  bool omit_label = false; // label is stored in the state's jump table
  const output_t *output = nullptr;
  const output_t *state_output = nullptr;

  size_t byte_size() const {
    auto sz = 1u;
    if (!omit_label && ope.label_index(need_output, need_state_output) == 0) {
      sz += 1;
    }
    if (!ope.data.no_address) { sz += vb_encode_value_length(delta); }
    if (need_output) {
      if (ope.data.has_output) {
        sz += OutputTraits<output_t>::get_byte_value_size(*output);
      }
      if (need_state_output) {
        if (ope.data.has_state_output) {
          sz += OutputTraits<output_t>::get_byte_value_size(*state_output);
        }
      }
    }
    return sz;
  }

  void write(std::ostream &os) {
    if (need_output) {
      if (need_state_output) {
        if (ope.data.has_state_output) {
          OutputTraits<output_t>::write_byte_value(os, *state_output);
        }
      }
      if (ope.data.has_output) {
        OutputTraits<output_t>::write_byte_value(os, *output);
      }
    }
    if (!ope.data.no_address) {
      OutputTraits<uint32_t>::write_byte_value(os,
                                               static_cast<uint32_t>(delta));
    }
    if (!omit_label && ope.label_index(need_output, need_state_output) == 0) {
      os.write(&label, 1);
    }
    os.write(reinterpret_cast<const char *>(&ope.byte), sizeof(ope.byte));
  }
};

struct FstHeader {
  union {
    struct {
      unsigned output_type : 3;
      unsigned need_state_output : 1;
      unsigned jump_table_labels : 1; // jump tables carry a label array
      unsigned hub_table : 1; // hub state addresses are stored in a table
      unsigned reserved : 2;
    } data;

    uint8_t byte;
  } flags;

  uint32_t start_address = 0;
  char char_index[32] = {0};

  uint32_t hub_count = 0;
  const char *hub_table = nullptr;

  bool need_output = false;
  bool need_state_output = false;

  FstHeader() = default;

  FstHeader(OutputType output_type, bool need_state_output,
            size_t start_address, const std::vector<size_t> &char_index_table,
            size_t hub_count = 0)
      : flags{}, start_address(static_cast<uint32_t>(start_address)),
        hub_count(static_cast<uint32_t>(hub_count)),
        need_output{output_type != OutputType::none_t},
        need_state_output{need_state_output} {

    flags.data.output_type = static_cast<uint8_t>(output_type);
    flags.data.need_state_output = need_state_output;
    flags.data.jump_table_labels = 1;
    flags.data.hub_table = hub_count > 0;

    auto size = char_index_size();
    for (auto ch = 0u; ch < 256; ch++) {
      auto index = char_index_table[ch];
      if (0 < index && index < size) {
        char_index[index] = static_cast<char>(ch);
      }
    }
  }

  bool read(const char *byte_code, size_t byte_code_size) {
    auto remaining = byte_code_size;
    if (remaining < sizeof(uint8_t)) { return false; }

    auto p = byte_code + (byte_code_size - sizeof(uint8_t));
    flags.byte = *p--;

    // For performance
    need_output = // needed before char_index_size()
        static_cast<OutputType>(flags.data.output_type) != OutputType::none_t;
    need_state_output = flags.data.need_state_output;

    remaining -= sizeof(uint8_t);

    if (flags.data.hub_table) {
      if (remaining < sizeof(uint32_t)) { return false; }
      memcpy(&hub_count, p - (sizeof(uint32_t) - 1), sizeof(hub_count));
      p -= sizeof(uint32_t);
      remaining -= sizeof(uint32_t);
    }

    if (remaining < sizeof(uint32_t)) { return false; }

    memcpy(&start_address, p - (sizeof(uint32_t) - 1), sizeof(start_address));
    p -= sizeof(uint32_t);

    remaining -= sizeof(uint32_t);
    auto size = char_index_size();
    if (remaining < size) { return false; }

    memcpy(char_index, p - (size - 1), size);
    p -= size;
    remaining -= size;

    if (flags.data.hub_table) {
      if (remaining < hub_count * sizeof(uint32_t)) { return false; }
      hub_table = p - (hub_count * sizeof(uint32_t) - 1);
    }
    return true;
  }

  void write(std::ostream &os) {
    os.write(char_index, char_index_size());
    os.write(reinterpret_cast<char *>(&start_address), sizeof(start_address));
    if (flags.data.hub_table) {
      os.write(reinterpret_cast<char *>(&hub_count), sizeof(hub_count));
    }
    os.write(reinterpret_cast<char *>(&flags.byte), sizeof(flags.byte));
  }

  uint32_t hub_address(size_t index) const {
    uint32_t address;
    memcpy(&address, hub_table + index * sizeof(uint32_t), sizeof(address));
    return address;
  }

  size_t char_index_size() const {
    return FstOpe::char_index_size(need_output, need_state_output);
  }
};

}