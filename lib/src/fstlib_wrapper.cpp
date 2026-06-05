#include <tuple>

#include <fstd/fstlib_wrapper.h>
#include <fstd/logger.h>

using namespace std;
namespace fstd {

void show_error_message(fst::Result result, size_t line) {
  std::string error_message;

  switch (result) {
  case fst::Result::EmptyKey: error_message = "empty key"; break;
  case fst::Result::UnsortedKey: error_message = "unsorted key"; break;
  case fst::Result::DuplicateKey: error_message = "duplicate key"; break;
  default: error_message = "Unknown"; break;
  }
  LOG_ERROR("line {}: {}", line, error_message);
}

} // namespace fstd