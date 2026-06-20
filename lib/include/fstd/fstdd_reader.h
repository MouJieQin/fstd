#pragma once

#include <fstd/common.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <set>

namespace fstd {

class FstddReader {

public:
  FstddReader(const std::string &fstdd_path);

  ~FstddReader() {}

  operator bool() const;

  bool extract(const std::string &key, const std::string &dst_dir = "data");

  bool extract_all(const std::string &dst_dir = "data");

  const DdJsonHeader &get_header() const;

  const nlohmann::json &get_meta() const;

private:
  bool parse_fstdd(const std::string &fstdd_path);

  bool extract_impl(const std::string &key, const std::string &dst_dir);

private:
  const std::string fstdd_path_;
  bool is_valid_;
  DdJsonHeader md_json_header_;
  size_t key_size_;
  std::set<size_t> dup_idxes_;
  size_t bucket_size_;
  size_t block_size_;
  uint64_t comp_blocks_offset_;
  uint64_t block_index_offset_;
  uint64_t hash_bucket_offset_;
  uint64_t hash_index_offset_;
  uint64_t keys_offset_;
};
} // namespace fstd