#pragma once
#include <fstream>
#include <iostream>

#include <fstd/fstlib.h>
#include <fstd/logger.h>

namespace fstd {

template <typename VT>
inline std::pair<size_t, size_t>
write_hash_index(std::ostream &out,
                 const std::vector<std::pair<std::string, VT>> &key_value,
                 std::set<size_t> &dup_bucket_idxes_) {
  using namespace std;
  uint32_t bucket_size = 8;
  if (key_value.size() > 64) { bucket_size = key_value.size() / 8; }
  std::vector<uint64_t> hash_codes;
  hash_codes.reserve(key_value.size());
  for (size_t i = 0; i < key_value.size(); ++i) {
    uint64_t hash_code = fst::MurmurHash64B(key_value[i].first.c_str(),
                                            key_value[i].first.size(), 0);
    hash_codes.push_back(hash_code);
  }

  std::vector<size_t> sorted_indices(key_value.size());
  std::set<uint64_t> dup_hash_codes;
  auto duplicate_check = [&]() {
    std::iota(sorted_indices.begin(), sorted_indices.end(), 0);
    std::sort(
        sorted_indices.begin(), sorted_indices.end(),
        [&](size_t i, size_t j) { return hash_codes[i] < hash_codes[j]; });
    size_t dup_idx = sorted_indices[0];
    for (size_t i = 1; i < sorted_indices.size(); ++i) {
      if (hash_codes[dup_idx] == hash_codes[sorted_indices[i]]) {
        dup_hash_codes.insert(hash_codes[dup_idx]);
        std::cout << "dup_hash_code: " << hash_codes[dup_idx] << std::endl;
      }
      dup_idx = sorted_indices[i];
    }
  };
  duplicate_check();
  std::set<size_t> dup_bucket_idxes;
  for (uint64_t code : dup_hash_codes) {
    dup_bucket_idxes.insert(code % bucket_size);
  }

  std::vector<size_t> hash_bucket_count(bucket_size, 0);
  std::vector<std::vector<size_t>> hash_bucket_map(bucket_size,
                                                   std::vector<size_t>());
  for (size_t i = 0; i < sorted_indices.size(); ++i) {
    size_t index = hash_codes[sorted_indices[i]] % bucket_size;
    hash_bucket_count[index] += 1;
    hash_bucket_map[index].push_back(sorted_indices[i]);
  }

  auto statistic = [&]() {
    size_t max_count = 0;
    for (auto c : hash_bucket_count) {
      if (c > max_count) { max_count = c; }
    }
    std::vector<size_t> counts(max_count + 1, 0);
    for (auto c : hash_bucket_count) {
      counts[c] += 1;
    }
    std::cout << "buket size: " << bucket_size << "\n"
              << "file num: " << key_value.size() << std::endl;
    for (size_t i = 0; i < counts.size(); ++i) {
      std::cout << i << " : " << counts[i] << "\n";
    }
    std::cout << std::endl;
  };
  statistic();

  std::ostringstream oss(std::ios::binary);
  uint32_t pos = 0;
  uint32_t b_size = 0;
  uint32_t offset = 0;
  std::vector<uint64_t> hash_bucket(bucket_size, 0);
  for (size_t i = 0; i < bucket_size; ++i) {
    b_size = 0;
    // cout << "# " << i << " : \n";
    if (dup_bucket_idxes.find(i) == dup_bucket_idxes.end()) {
      for (size_t j = 0; j < hash_bucket_map[i].size(); ++j) {
        uint32_t key_index = hash_bucket_map[i][j];
        uint64_t hash_code = hash_codes[key_index];
        // cout << hash_code << " ";
        oss.write(reinterpret_cast<const char *>(&hash_code),
                  sizeof(hash_code));
      }
      for (size_t j = 0; j < hash_bucket_map[i].size(); ++j) {
        uint32_t key_index = hash_bucket_map[i][j];
        oss.write(reinterpret_cast<const char *>(&key_value[key_index].second),
                  sizeof(VT));
      }
      // cout << endl;
      b_size = hash_bucket_map[i].size() * (sizeof(uint64_t) + sizeof(VT));
    } else {
      for (size_t j = 0; j < hash_bucket_map[i].size(); ++j) {
        uint32_t key_index = hash_bucket_map[i][j];
        std::string key = key_value[key_index].first;
        oss.write(key.c_str(), key.size() + 1);
        oss.write(reinterpret_cast<const char *>(&key_value[key_index].second),
                  sizeof(VT));
        b_size += key.size() + 1 + sizeof(VT);
      }
    }
    // LOG_INFO("pos: {} offset: {}", pos, b_size);
    hash_bucket[i] =
        (static_cast<uint64_t>(pos) << 24) | static_cast<uint64_t>(b_size);
    offset += b_size;
    pos = offset;
  }

  const string bucket_data(oss.str());
  out.write(bucket_data.c_str(), bucket_data.size());
  out.write(reinterpret_cast<const char *>(&hash_bucket[0]),
            sizeof(hash_bucket[0]) * hash_bucket.size());
  dup_bucket_idxes_.swap(dup_bucket_idxes);
  return {bucket_size, bucket_data.size()};
}

template <typename VT>
inline bool
read_hash_index(std::ifstream &in, const std::string &key, VT &value,
                const std::set<size_t> &dup_idxes, const size_t bucket_size,
                const size_t bucket_offset, const size_t index_offset) {
  uint64_t hash_code = fst::MurmurHash64B(key.c_str(), key.size(), 0);
  std::cout << "hash_code: " << hash_code << std::endl;
  std::cout << "bucket_size: " << bucket_size << std::endl;
  size_t index = hash_code % bucket_size;
  std::cout << "index: " << index << std::endl;
  uint64_t hash_bucket_index = 0;
  in.seekg(index * sizeof(uint64_t) + index_offset);
  in.read(reinterpret_cast<char *>(&hash_bucket_index),
          sizeof(hash_bucket_index));
  if (hash_bucket_index == 0) {
    std::cout << "key not found" << std::endl;
    return false;
  }
  uint64_t pos = hash_bucket_index >> 24;
  uint32_t offset = hash_bucket_index & 0xFFFFFF;
  // std::cout << "pos: " << pos << std::endl;
  // std::cout << "offset: " << offset << std::endl;

  in.seekg(pos + bucket_offset);
  if (dup_idxes.find(index) == dup_idxes.end()) {
    size_t size = offset / (sizeof(uint64_t) + sizeof(VT));
    // LOG_INFO("size: {}", size);
    std::vector<uint64_t> hash_codes(size);
    std::vector<VT> values(size);
    in.read(reinterpret_cast<char *>(hash_codes.data()),
            size * sizeof(uint64_t));
    in.read(reinterpret_cast<char *>(values.data()), size * sizeof(VT));
    for (size_t i = 0; i < size; ++i) {
      // std::cout << hash_codes[i] << " ";
      if (hash_code == hash_codes[i]) {
        value = values[i];
        std::cout << "key found" << std::endl;
        std::cout << hash_code << " : " << value << std::endl;
        return true;
      }
    }
    std::cout << std::endl;
  } else {
    std::vector<char> buffer(offset);
    std::cout << "buffer size: " << offset << std::endl;
    in.read(buffer.data(), offset);
    std::string k;
    uint32_t start_index = 0;
    uint32_t k_size = 0;
    uint32_t i = 0;
    for (; i < offset;) {
      if (buffer[i] == '\0') {
        k = std::string(buffer.data() + start_index, k_size);
        value = *reinterpret_cast<VT *>(&buffer[i + 1]);
        std::cout << k << " : " << value << std::endl;
        if (k == key) {
          std::cout << "key found" << std::endl;
          return true;
        }
        start_index = i + sizeof(VT) + 1;
        k_size = 0;
        i += sizeof(VT) + 1;
      } else {
        k_size += 1;
        ++i;
      }
    }
  }
  std::cout << "key not found" << std::endl;
  return false;
}

template <typename VT>
inline bool
read_hash_index(const std::string &file_path, const std::string &key, VT &value,
                const std::set<size_t> &dup_idxes, const size_t bucket_size,
                const size_t bucket_offset, const size_t index_offset) {
  std::ifstream in(file_path, std::ios::binary);
  if (!in) {
    LOG_ERROR("Cannot open the file: {}", file_path);
    return false;
  }
  return read_hash_index(in, key, value, dup_idxes, bucket_size, bucket_offset,
                         index_offset);
}
} // namespace fstd