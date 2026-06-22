//
//  fstlib.h
//
//  Copyright (c) 2022 Yuji Hirose. All rights reserved.
//  MIT License
//

#pragma once
#include <fstlib/map.h>
#include <fstlib/matcher_utility.h>
#include <fstlib/set.h>

namespace fst {

//-----------------------------------------------------------------------------
// decompile
//-----------------------------------------------------------------------------

inline void decompile(const char *byte_code, size_t byte_code_size,
                      std::ostream &out, bool need_output = true) {

  auto type = get_output_type(byte_code, byte_code_size);

  if (type == OutputType::uint32_t) {
    map<uint32_t> matcher(byte_code, byte_code_size);
    if (matcher) {
      matcher.enumerate([&](const auto &word, auto output) {
        if (need_output) {
          out << word << '\t' << output << std::endl;
        } else {
          out << word << std::endl;
        }
      });
    }
  } else if (type == OutputType::uint64_t) {
    map<uint64_t> matcher(byte_code, byte_code_size);
    if (matcher) {
      matcher.enumerate([&](const auto &word, auto output) {
        if (need_output) {
          out << word << '\t' << output << std::endl;
        } else {
          out << word << std::endl;
        }
      });
    }
  } else if (type == OutputType::string) {
    map<std::string> matcher(byte_code, byte_code_size);
    if (matcher) {
      matcher.enumerate([&](const auto &word, auto output) {
        if (need_output) {
          out << word << '\t' << output << std::endl;
        } else {
          out << word << std::endl;
        }
      });
    }
  } else if (type == OutputType::none_t) {
    set matcher(byte_code, byte_code_size);
    if (matcher) {
      matcher.enumerate(
          [&](const auto &word, auto output) { out << word << std::endl; });
    }
  }
}

template <typename T>
inline void decompile(const T &byte_code, std::ostream &out,
                      bool need_output = true) {
  decompile(byte_code.data(), byte_code.size(), out, need_output);
}

} // namespace fst