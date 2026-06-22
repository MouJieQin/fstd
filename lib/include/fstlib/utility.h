//
//  fstlib.h
//
//  Copyright (c) 2022 Yuji Hirose. All rights reserved.
//  MIT License
//

#pragma once
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace fst {

//-----------------------------------------------------------------------------
// variable byte encoding
//-----------------------------------------------------------------------------

template <typename Val> inline size_t vb_encode_value_length(Val n) {
  auto len = 0u;
  while (n >= 128) {
    len++;
    n >>= 7;
  }
  len++;
  return len;
}

template <typename Val> inline size_t vb_encode_value(Val n, char *out) {
  auto len = 0u;
  while (n >= 128) {
    out[len] = static_cast<char>(n & 0x7f);
    len++;
    n >>= 7;
  }
  out[len] = static_cast<char>(n + 128);
  len++;
  return len;
}

template <typename Val, typename Cont> void vb_encode_value(Val n, Cont &out) {
  while (n >= 128) {
    out.push_back(static_cast<typename Cont::value_type>(n & 0x7f));
    n >>= 7;
  }
  out.push_back(static_cast<typename Cont::value_type>(n + 128));
}

template <typename Val>
inline size_t vb_encode_value_reverse(Val n, char *out) {
  auto len = vb_encode_value(n, out);
  for (auto i = 0u; i < len / 2; i++) {
    std::swap(out[i], out[len - i - 1]);
  }
  return len;
}

template <typename Val>
inline size_t vb_encode_value_reverse(Val n, std::ostream &os) {
  char buf[16];
  auto len = vb_encode_value_reverse(n, buf);
  os.write(buf, len);
  return len;
}

template <typename Val>
inline size_t vb_decode_value_reverse(const char *data, Val &n) {
  auto p = reinterpret_cast<const uint8_t *>(data);
  auto i = 0;
  n = 0;
  auto cnt = 0u;
  while (p[i] < 128) {
    n += (static_cast<Val>(p[i--]) << (7 * cnt++));
  }
  n += (static_cast<Val>(p[i--]) - 128) << (7 * cnt);
  return i * -1;
}

//-----------------------------------------------------------------------------
// lower_bound_index
//-----------------------------------------------------------------------------

template <class T>
inline size_t lower_bound_index(size_t first, size_t last, T less) {
  auto len = last - first;

  while (len > 0) {
    auto half = len >> 1;
    auto middle = first + half;

    if (less(middle)) {
      first = middle;
      first++;
      len = len - half - 1;
    } else {
      len = half;
    }
  }

  return first;
}

//-----------------------------------------------------------------------------
// MurmurHash64B - 64-bit MurmurHash2 for 32-bit platforms
//
// URL:: https://github.com/aappleby/smhasher/blob/master/src/MurmurHash2.cpp
// License: Public Domain
//-----------------------------------------------------------------------------

inline uint64_t MurmurHash64B(const void *key, size_t len, uint64_t seed) {
  const auto m = uint32_t(0x5bd1e995);
  const auto r = 24u;

  auto h1 = static_cast<uint32_t>(seed) ^ static_cast<uint32_t>(len);
  auto h2 = static_cast<uint32_t>(seed >> 32);

  auto data = reinterpret_cast<const uint32_t *>(key);

  while (len >= 8) {
    auto k1 = *data++;
    k1 *= m;
    k1 ^= k1 >> r;
    k1 *= m;
    h1 *= m;
    h1 ^= k1;
    len -= 4;

    auto k2 = *data++;
    k2 *= m;
    k2 ^= k2 >> r;
    k2 *= m;
    h2 *= m;
    h2 ^= k2;
    len -= 4;
  }

  if (len >= 4) {
    auto k1 = *data++;
    k1 *= m;
    k1 ^= k1 >> r;
    k1 *= m;
    h1 *= m;
    h1 ^= k1;
    len -= 4;
  }

  switch (len) {
  case 3: h2 ^= reinterpret_cast<const unsigned char *>(data)[2] << 16;
  case 2: h2 ^= reinterpret_cast<const unsigned char *>(data)[1] << 8;
  case 1: h2 ^= reinterpret_cast<const unsigned char *>(data)[0]; h2 *= m;
  };

  h1 ^= h2 >> 18;
  h1 *= m;
  h2 ^= h1 >> 22;
  h2 *= m;
  h1 ^= h2 >> 17;
  h1 *= m;
  h2 ^= h1 >> 19;
  h2 *= m;

  auto h = static_cast<uint64_t>(h1);
  h = (h << 32) | h2;
  return h;
}

//-----------------------------------------------------------------------------
// char_to_string
//-----------------------------------------------------------------------------

inline std::string char_to_string(char arc) {
  std::stringstream ss;
  if (arc < 0x20) {
    ss << std::hex << std::setfill('0') << std::setw(2) << (int)(uint8_t)arc
       << std::dec;
  } else {
    ss << arc;
  }
  return ss.str();
}

//-----------------------------------------------------------------------------
// get_common_prefix_length
//-----------------------------------------------------------------------------

inline size_t get_common_prefix_length(std::string_view s1,
                                       std::string_view s2) {
  auto i = 0u;
  while (i < s1.size() && i < s2.size() && s1[i] == s2[i]) {
    i++;
  }
  return i;
}

//-----------------------------------------------------------------------------
// get_prefix_length
//-----------------------------------------------------------------------------

inline bool get_prefix_length(std::string_view s1, std::string_view s2,
                              size_t &l) {
  l = 0;
  while (l < s1.size() && l < s2.size()) {
    auto ch1 = static_cast<uint8_t>(s1[l]);
    auto ch2 = static_cast<uint8_t>(s2[l]);
    if (ch1 < ch2) { break; }
    if (ch1 > ch2) { return false; }
    l++;
  }
  return true;
}

//-----------------------------------------------------------------------------
// hash_bytes (FNV-1a)
//-----------------------------------------------------------------------------

inline void hash_bytes(uint64_t &h, const void *data, size_t len) {
  auto p = static_cast<const uint8_t *>(data);
  for (size_t i = 0; i < len; i++) {
    h = (h ^ p[i]) * 0x100000001b3ULL;
  }
}

constexpr uint64_t kFnvBasis = 0xcbf29ce484222325ULL;

} // namespace fst