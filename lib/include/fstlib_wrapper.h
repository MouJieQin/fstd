#pragma once

#include "fstlib.h"
#include <sstream>

namespace fstd {

inline std::pair<fst::Result, size_t>
compile(const std::vector<std::pair<std::string, uint64_t>> &input,
        std::ostream &os, bool sorted, bool verbose = false);

void show_error_message(fst::Result result, size_t line);

bool compile_fst(std::vector<std::pair<std::string, uint64_t>> &input,
                 std::ostringstream &oss_out, bool opt_sorted,
                 bool opt_verbose);

} // namespace fstd