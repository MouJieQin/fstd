#pragma once
#include <fstream>

#include <nlohmann/json.hpp>

namespace fstd {

#define FSTD_VERSION "0.1.0"

using MxJsonHeader = nlohmann::json;
using MdJsonHeader = nlohmann::json;

struct HeaderSizeRecord {
  HeaderSizeRecord() = default;
  HeaderSizeRecord(uint32_t original_size, uint32_t compressed_size);
  uint32_t original_size;
  uint32_t compressed_size;
};

std::string get_current_date();

bool handle_meta(const nlohmann::json &meta, const nlohmann::json &meta_default,
                 nlohmann::json &header);

bool compress_to_buffer(const std::string &src, size_t src_size,
                        std::vector<char> &dst, int compression_level);

bool compress_to_buffer(const char *src, size_t src_size,
                        std::vector<char> &dst, int compression_level);

bool decompress_to_buffer(const void *src, size_t compressed_size,
                          size_t original_size, std::vector<char> &dst);

bool parse_header(std::ifstream &ins, const size_t file_size,
                  nlohmann::json &header);

template <typename T>
bool decompress(std::istream &ins, const std::string &block_name,
                const MxJsonHeader &mx_json_header_, std::vector<T> &con) {
  const nlohmann::json &json_block = mx_json_header_[block_name];
  int compress_level = json_block["compress_level"];
  uint64_t offset = json_block["offset"];
  uint64_t original_size = json_block["original_size"];
  std::vector<T> tmp_con;
  tmp_con.resize(original_size / sizeof(T));
  if (compress_level == 0) {
    ins.seekg(offset);
    ins.read(reinterpret_cast<char *>(tmp_con.data()), tmp_con.size());
  } else {
    uint64_t compressed_size = json_block["compressed_size"];
    std::vector<char> compressed_block(compressed_size);
    ins.seekg(offset);
    ins.read(compressed_block.data(), compressed_block.size());
    std::vector<char> dst_buff(original_size);
    bool res =
        decompress_to_buffer(compressed_block.data(), compressed_block.size(),
                             original_size, dst_buff);
    if (!res) { return false; }
    memcpy(tmp_con.data(), dst_buff.data(), dst_buff.size());
  }
  con.swap(tmp_con);
  return true;
}

// ------------------------------
// 通用：根据元素大小排序索引
// ------------------------------
template <typename Cont_p>
inline std::vector<size_t> sort_indexes(const Cont_p &input) {
  std::vector<size_t> indices(input.size());
  std::iota(indices.begin(), indices.end(), 0);
  std::sort(indices.begin(), indices.end(),
            [&](size_t i, size_t j) { return input[i] < input[j]; });
  return indices;
}

// ------------------------------
// 特化：针对 vector<pair<...>> 按 first 排序
// ------------------------------
template <typename T1, typename T2>
inline std::vector<size_t>
sort_indexes(const std::vector<std::pair<T1, T2>> &input) {
  std::vector<size_t> indices(input.size());
  std::iota(indices.begin(), indices.end(), 0);
  std::sort(indices.begin(), indices.end(), [&](size_t i, size_t j) {
    return input[i].first < input[j].first;
  });
  return indices;
}

// ------------------------------
// 通用版本：支持 pair 等结构
// ------------------------------
template <typename T1, typename T2>
inline std::vector<std::string>
sort_container(std::vector<std::pair<T1, T2>> &&input) {
  auto indices = sort_indexes(input);
  std::vector<std::string> res;
  res.reserve(indices.size());

  for (size_t i : indices) {
    res.emplace_back(std::move(input[i].first));
  }
  return res;
}

inline std::vector<std::string>
sort_container(std::vector<std::string> &&input) {
  auto indices = sort_indexes(input);
  std::vector<std::string> res;
  res.reserve(indices.size());

  for (size_t i : indices) {
    res.emplace_back(std::move(input[i]));
  }
  return res;
}
} // namespace fstd