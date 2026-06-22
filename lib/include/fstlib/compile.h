//
//  fstlib.h
//
//  Copyright (c) 2022 Yuji Hirose. All rights reserved.
//  MIT License
//

#pragma once
#include <fstlib/build_fst.h>
#include <fstlib/writer.h>

namespace fst {

template <typename output_t, typename Input>
inline std::pair<Result, size_t> compile(const Input &input, std::ostream &os,
                                         bool sorted, bool verbose = false) {
  FstWriter<output_t, true> writer(
      os, true, false, verbose,
      [&](const auto &feeder) {
        for (const auto &[word, _] : input) {
          feeder(word);
        }
      },
      /*single_pass=*/true);
  return build_fst<output_t>(input, writer, true, sorted,
                             /*keep_all_states=*/true);
}

template <typename Input>
inline std::pair<Result, size_t> compile(const Input &input, std::ostream &os,
                                         bool need_output, bool sorted,
                                         bool verbose = false) {
  FstWriter<uint32_t, false> writer(
      os, need_output, false, verbose,
      [&](const auto &feeder) {
        for (const auto &word : input) {
          feeder(word);
        }
      },
      /*single_pass=*/true);
  return build_fst(input, writer, need_output, sorted,
                   /*keep_all_states=*/true);
}

template <typename output_t, typename Input>
inline std::pair<Result, size_t> dump(const Input &input, std::ostream &os,
                                      bool sorted, bool verbose = false) {
  FstWriter<output_t, true> writer(os, true, true, verbose,
                                   [&](const auto &feeder) {
                                     for (const auto &[word, _] : input) {
                                       feeder(word);
                                     }
                                   });
  return build_fst<output_t>(input, writer, true, sorted);
}

template <typename Input>
inline std::pair<Result, size_t> dump(const Input &input, std::ostream &os,
                                      bool need_output, bool sorted,
                                      bool verbose = false) {
  FstWriter<uint32_t, true> writer(os, need_output, true, verbose,
                                   [&](const auto &feeder) {
                                     for (const auto &word : input) {
                                       feeder(word);
                                     }
                                   });
  return build_fst(input, writer, need_output, sorted);
}

template <typename output_t, typename Input>
inline std::pair<Result, size_t> dot(const Input &input, std::ostream &os,
                                     bool sorted) {

  DotWriter<output_t> writer(os);
  return build_fst<output_t>(input, writer, true, sorted);
}

template <typename Input>
inline std::pair<Result, size_t> dot(const Input &input, std::ostream &os,
                                     bool need_output, bool sorted) {
  DotWriter<uint32_t> writer(os);
  return build_fst(input, writer, need_output, sorted);
}

} // namespace fst