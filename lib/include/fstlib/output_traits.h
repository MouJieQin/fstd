//
//  fstlib.h
//
//  Copyright (c) 2022 Yuji Hirose. All rights reserved.
//  MIT License
//

#pragma once
#include <fstlib/utility.h>

namespace fst {
//-----------------------------------------------------------------------------
// OutputTraits
//-----------------------------------------------------------------------------

using none_t = int;

enum class OutputType {
  invalid = -1,
  none_t,
  uint32_t,
  uint64_t,
  string,
  uint32bit,
  uint64bit
};

template <typename output_t> struct OutputTraits {};

template <> struct OutputTraits<none_t> {
  using value_type = none_t;

  static OutputType type() { return OutputType::none_t; }

  static bool empty(value_type val) { return val == 0; }

  static value_type init_value() { return 0; }

  static void hash_value(uint64_t &, value_type) {}

  static size_t read_byte_value(const char *, value_type &) { return 0; }
};

template <> struct OutputTraits<uint32_t> {
  using value_type = uint32_t;

  static OutputType type() { return OutputType::uint32_t; }

  static bool empty(value_type val) { return val == 0; }

  static value_type init_value() { return 0; }

  static std::string to_string(value_type val) { return std::to_string(val); }

  static void prepend_value(value_type &base, value_type val) { base += val; }

  static value_type get_suffix(value_type a, value_type b) { return a - b; }

  static value_type get_common_prefix(value_type a, value_type b) {
    return std::min(a, b);
  }

  template <typename T> static size_t write_value(T &buff, value_type val) {
    auto p = reinterpret_cast<const char *>(&val);
    buff.insert(buff.begin(), p, p + sizeof(val));
    return sizeof(val);
  }

  static void hash_value(uint64_t &h, value_type val) {
    hash_bytes(h, &val, sizeof(val));
  }

  static size_t get_byte_value_size(value_type val) {
    return vb_encode_value_length(val);
  }

  static void write_byte_value(std::ostream &os, value_type val) {
    vb_encode_value_reverse(val, os);
  }

  static size_t read_byte_value(const char *p, value_type &val) {
    return vb_decode_value_reverse(p, val);
  }
};

template <> struct OutputTraits<uint64_t> {
  using value_type = uint64_t;

  static OutputType type() { return OutputType::uint64_t; }

  static bool empty(value_type val) { return val == 0; }

  static value_type init_value() { return 0; }

  static std::string to_string(value_type val) { return std::to_string(val); }

  static void prepend_value(value_type &base, value_type val) { base += val; }

  static value_type get_suffix(value_type a, value_type b) { return a - b; }

  static value_type get_common_prefix(value_type a, value_type b) {
    return std::min(a, b);
  }

  template <typename T> static size_t write_value(T &buff, value_type val) {
    auto p = reinterpret_cast<const char *>(&val);
    buff.insert(buff.begin(), p, p + sizeof(val));
    return sizeof(val);
  }

  static void hash_value(uint64_t &h, value_type val) {
    hash_bytes(h, &val, sizeof(val));
  }

  static size_t get_byte_value_size(value_type val) {
    return vb_encode_value_length(val);
  }

  static void write_byte_value(std::ostream &os, value_type val) {
    vb_encode_value_reverse(val, os);
  }

  static size_t read_byte_value(const char *p, value_type &val) {
    return vb_decode_value_reverse(p, val);
  }
};

struct uint32bit {
  uint32bit() : bits(0) {}
  uint32bit(uint32_t val) : bits(val) {}
  uint32bit operator+(const uint32bit &other) const {
    return uint32bit(bits | other.bits);
  }

  bool operator==(const uint32bit &other) const { return bits == other.bits; }
  uint32_t bits;
};

inline void operator+=(uint32bit &lhs, const uint32bit &rhs) {
  lhs = lhs + rhs;
}

inline std::ostream &operator<<(std::ostream &os, const uint32bit &val) {
  os << val.bits;
  return os;
}

template <> struct OutputTraits<uint32bit> {
  using value_type = uint32bit;

  static OutputType type() { return OutputType::uint32bit; }

  static bool empty(value_type val) { return val.bits == 0; }

  static value_type init_value() { return 0; }

  static std::string to_string(value_type val) {
    return std::to_string(val.bits);
  }

  static void prepend_value(value_type &base, value_type val) {
    base.bits |= val.bits;
  }

  static value_type get_suffix(value_type a, value_type b) {
    return a.bits & ~b.bits;
  }

  static value_type get_common_prefix(value_type a, value_type b) {
    return a.bits & b.bits;
  }

  template <typename T> static size_t write_value(T &buff, value_type val) {
    auto p = reinterpret_cast<const char *>(&val);
    buff.insert(buff.begin(), p, p + sizeof(val));
    return sizeof(val);
  }

  static void hash_value(uint64_t &h, value_type val) {
    hash_bytes(h, &val.bits, sizeof(val.bits));
  }

  static size_t get_byte_value_size(value_type val) {
    return vb_encode_value_length(val.bits);
  }

  static void write_byte_value(std::ostream &os, value_type val) {
    vb_encode_value_reverse(val.bits, os);
  }

  static size_t read_byte_value(const char *p, value_type &val) {
    return vb_decode_value_reverse(p, val.bits);
  }
};

struct uint64bit {
  uint64bit() : bits(0) {}
  uint64bit(uint64_t val) : bits(val) {}
  uint64bit operator+(const uint64bit &other) const {
    return uint64bit(bits | other.bits);
  }

  bool operator==(const uint64bit &other) const { return bits == other.bits; }
  uint64_t bits;
};

inline void operator+=(uint64bit &lhs, const uint64bit &rhs) {
  lhs = lhs + rhs;
}

inline std::ostream &operator<<(std::ostream &os, const uint64bit &val) {
  os << val.bits;
  return os;
}

template <> struct OutputTraits<uint64bit> {
  using value_type = uint64bit;

  static OutputType type() { return OutputType::uint64bit; }

  static bool empty(value_type val) { return val.bits == 0; }

  static value_type init_value() { return 0; }

  static std::string to_string(value_type val) {
    return std::to_string(val.bits);
  }

  static void prepend_value(value_type &base, value_type val) {
    base.bits |= val.bits;
  }

  static value_type get_suffix(value_type a, value_type b) {
    return a.bits & ~b.bits;
  }

  static value_type get_common_prefix(value_type a, value_type b) {
    return a.bits & b.bits;
  }

  template <typename T> static size_t write_value(T &buff, value_type val) {
    auto p = reinterpret_cast<const char *>(&val);
    buff.insert(buff.begin(), p, p + sizeof(val));
    return sizeof(val);
  }

  static void hash_value(uint64_t &h, value_type val) {
    hash_bytes(h, &val.bits, sizeof(val.bits));
  }

  static size_t get_byte_value_size(value_type val) {
    return vb_encode_value_length(val.bits);
  }

  static void write_byte_value(std::ostream &os, value_type val) {
    vb_encode_value_reverse(val.bits, os);
  }

  static size_t read_byte_value(const char *p, value_type &val) {
    return vb_decode_value_reverse(p, val.bits);
  }

  static bool output_validator(uint64bit output, uint64_t mask) {
    return (output.bits & mask) != 0;
  }
};

template <> struct OutputTraits<std::string> {
  using value_type = std::string;

  static OutputType type() { return OutputType::string; }

  static bool empty(const value_type &val) { return val.empty(); }

  static value_type init_value() { return std::string(); }

  static value_type to_string(const value_type &val) { return val; }

  static void prepend_value(value_type &base, const value_type &val) {
    base.insert(0, val);
  }

  static value_type get_suffix(const value_type &a, const value_type &b) {
    return a.substr(b.size());
  }

  static value_type get_common_prefix(const value_type &a,
                                      const value_type &b) {
    return a.substr(0, get_common_prefix_length(a, b));
  }

  template <typename T> static size_t write_value(T &buff, value_type val) {
    buff.insert(buff.begin(), val.data(), val.data() + val.size());
    return val.size();
  }

  static void hash_value(uint64_t &h, const value_type &val) {
    hash_bytes(h, val.data(), val.size());
  }

  static size_t get_byte_value_size(const value_type &val) {
    return vb_encode_value_length(val.size()) + val.size();
  }

  static void write_byte_value(std::ostream &os, const value_type &val) {
    os.write(val.data(), val.size());
    vb_encode_value_reverse(static_cast<uint32_t>(val.size()), os);
  }

  static size_t read_byte_value(const char *p, value_type &val) {
    uint32_t str_len = 0;
    auto vb_len = vb_decode_value_reverse(p, str_len);

    val.resize(str_len);
    memcpy(val.data(), p - vb_len - str_len + 1, str_len);

    return vb_len + str_len;
  }
};

} // namespace fst