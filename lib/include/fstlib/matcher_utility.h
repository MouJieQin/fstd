//
//  fstlib.h
//
//  Copyright (c) 2022 Yuji Hirose. All rights reserved.
//  MIT License
//

#pragma once

#include <fstlib/byte_code.h>

namespace fst {

//-----------------------------------------------------------------------------
// get_output_type
//-----------------------------------------------------------------------------

inline OutputType get_output_type(const char *byte_code,
                                  size_t byte_code_size) {
  FstHeader header;
  if (!header.read(byte_code, byte_code_size)) { return OutputType::invalid; }
  return static_cast<OutputType>(header.flags.data.output_type);
}

template <typename T> OutputType get_output_type(const T &byte_code) {
  return get_output_type(byte_code.data(), byte_code.size());
}

//-----------------------------------------------------------------------------
// levenshtein_distance
//-----------------------------------------------------------------------------

inline double cost_replace(std::string_view from, size_t i, std::string_view to,
                           size_t j) {
  auto c1 = from[i];
  auto c2 = to[j];

  if (c1 == c2) return 0;

  // one char similar sound...
  {
    const char *similers[] = {
        "ao",
        "ae",
        "iy",
        "ou",
    };

    for (auto s : similers) {
      if ((c1 == s[0] && c2 == s[1]) || (c1 == s[1] && c2 == s[0])) return 0.25;
    }
  }

  if (i + 1 < from.size() && j + 1 < to.size()) {
    auto cn1 = from[i + 1];
    auto cn2 = to[j + 1];

    // Transposed chars...
    if (c1 == cn2 && c2 == cn1) return 0;

    // two chars similar sound...
    {
      const char *similers[] = {
          "irer",
          "urer",
          "irur",
          "erar",
      };

      for (auto s : similers) {
        if ((c1 == s[0] && cn1 == s[1] && c2 == s[2] && cn2 == s[3]) ||
            (c1 == s[2] && cn1 == s[3] && c2 == s[0] && cn2 == s[1]))
          return 0;
      }
    }
  }

  return 1.0;
}

inline double cost_insert(std::string_view to, size_t j) {
  if (j + 1 < to.size() && to[j] == to[j + 1]) return 0.5;
  return 1.0;
}

inline double cost_delete(std::string_view from, size_t i) {
  if (i + 1 < from.size() && from[i] == from[i + 1]) return 0.5;
  return 1.0;
}

inline double levenshtein_distance(std::string_view from, std::string_view to) {
  std::vector<std::vector<double>> m(from.size() + 1);

  for (size_t i = 0; i < m.size(); i++)
    m[i].assign(to.size() + 1, 0);

  for (size_t i = 0; i < m.size(); i++)
    m[i][0] = i;
  for (size_t j = 0; j < m[0].size(); j++)
    m[0][j] = j;

  for (size_t i = 0; i < from.size(); i++)
    for (size_t j = 0; j < to.size(); j++) {
      m[i + 1][j + 1] =
          std::min(m[i][j + 1] + cost_insert(to, j),                  // insert
                   std::min(m[i + 1][j] + cost_delete(from, i),       // delete
                            m[i][j] + cost_replace(from, i, to, j))); // replace
    }

  auto d = m.back().back();
  return 1.0 - (d / std::max<double>(from.size(), to.size()));
}

inline size_t max_range(std::string_view s1, std::string_view s2) {
  return (std::max(s1.length(), s2.length()) / 2) - 1;
}

inline bool common_string(std::string_view s1, std::string_view s2,
                          std::string &cs) {
  auto r = max_range(s1, s2);

  for (size_t i = 0; i < s1.length(); i++) {
    auto beg = std::max<int>(0, (int)i - (int)r);
    auto end = std::min(s2.length(), (i + r + 1));

    auto c1 = s1[i];
    for (size_t j = beg; j < end; j++) {
      if (c1 == s2[j]) {
        cs += c1;
        break;
      }
    }
  }

  return !cs.empty();
}

inline size_t commn_prefix_len(std::string_view s1, std::string_view s2) {
  auto len = std::min(s1.length(), s2.length());
  size_t i = 0;
  for (; i < len && s1[i] == s2[i]; i++)
    ;
  return i;
}

//-----------------------------------------------------------------------------
// jaro_winkler_distance
//-----------------------------------------------------------------------------

inline double jaro_distance(std::string_view s1, std::string_view s2) {
  std::string cs1;
  if (!common_string(s1, s2, cs1)) return 0;

  std::string cs2;
  if (!common_string(s2, s1, cs2)) return 0;

  double t = 0;
  auto end = std::min(cs1.length(), cs2.length());
  for (size_t i = 0; i < end; i++)
    if (cs1[i] != cs2[i]) t += 1;
  t /= 2;

  auto m = static_cast<double>(cs1.length());

  return ((m / s1.length()) + (m / s2.length()) + ((m - t) / m)) / 3;
}

inline double jaro_winkler_distance(std::string_view s1, std::string_view s2) {
  double dj = jaro_distance(s1, s2);
  if (dj) {
    auto l = static_cast<double>(commn_prefix_len(s1, s2));
    const auto p = 0.1;
    return dj + (l * p * (1.0 - dj));
  }
  return 0.0;
}

//-----------------------------------------------------------------------------
// UTF-8 -> UTF-32 decoder
//-----------------------------------------------------------------------------

inline bool decode_codepoint(std::string_view s8, char32_t &cp) {
  auto l = s8.size();
  if (l) {
    uint8_t b = s8[0];
    if ((b & 0x80) == 0) {
      cp = b;
      return true;
    } else if ((b & 0xE0) == 0xC0) {
      if (l >= 2) {
        cp = ((static_cast<char32_t>(s8[0] & 0x1F)) << 6) |
             (static_cast<char32_t>(s8[1] & 0x3F));
        return true;
      }
    } else if ((b & 0xF0) == 0xE0) {
      if (l >= 3) {
        cp = ((static_cast<char32_t>(s8[0] & 0x0F)) << 12) |
             ((static_cast<char32_t>(s8[1] & 0x3F)) << 6) |
             (static_cast<char32_t>(s8[2] & 0x3F));
        return true;
      }
    } else if ((b & 0xF8) == 0xF0) {
      if (l >= 4) {
        cp = ((static_cast<char32_t>(s8[0] & 0x07)) << 18) |
             ((static_cast<char32_t>(s8[1] & 0x3F)) << 12) |
             ((static_cast<char32_t>(s8[2] & 0x3F)) << 6) |
             (static_cast<char32_t>(s8[3] & 0x3F));
        return true;
      }
    }
  }
  return false;
}

// Simple UTF-8 stream validator helper.
inline bool u8_validator(std::string_view s8) {
  auto l = s8.size();
  if (l) {
    uint8_t b = s8[0];
    if ((b & 0x80) == 0) {
      return true;
    } else if ((b & 0xE0) == 0xC0) {
      if (l >= 2) { return true; }
    } else if ((b & 0xF0) == 0xE0) {
      if (l >= 3) { return true; }
    } else if ((b & 0xF8) == 0xF0) {
      if (l >= 4) { return true; }
    }
  }
  return false;
}

inline std::u32string decode(std::string_view s8) {
  std::u32string out;
  size_t i = 0;
  while (i < s8.size()) {
    auto beg = i++;
    while (i < s8.size() && (s8[i] & 0xc0) == 0x80) {
      i++;
    }
    char32_t cp;
    decode_codepoint(s8.substr(beg, i - beg), cp);
    out += cp;
  }
  return out;
}

inline size_t calc_c_len(std::string_view s8) {
  std::string u8code_ = "";
  size_t c_len = 0;
  for (size_t i = 0; i < s8.size(); ++i) {
    u8code_ += s8[i];
    if (u8_validator(u8code_)) {
      u8code_.clear();
      c_len += 1;
    }
  }
  return c_len;
}

} // namespace fst