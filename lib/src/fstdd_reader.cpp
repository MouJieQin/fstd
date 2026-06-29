#include <fstd/common.h>
#include <fstd/fstdd_compressor.h>
#include <fstd/fstdd_reader.h>
#include <fstd/hash_index.h>
#include <fstd/logger.h>

namespace fstd {

namespace fs = std::filesystem;
using namespace std;
using json = nlohmann::json;

FstddReader::FstddReader(const std::string &fstdd_path)
    : fstdd_path_(fstdd_path) {
  if (!parse_fstdd(fstdd_path_)) {
    is_valid_ = false;
    return;
  }
  is_valid_ = true;
}

FstddReader::operator bool() const { return is_valid_; }

const DdJsonHeader &FstddReader::get_header() const { return md_json_header_; }

const json &FstddReader::get_meta() const { return md_json_header_["meta"]; }

bool FstddReader::parse_fstdd(const std::string &fstdd_path) {
  std::ifstream ins(fstdd_path, std::ios::binary | std::ios::ate);
  if (!ins) {
    LOG_ERROR("Cannot open the file: {}", fstdd_path);
    return false;
  }
  size_t fstdd_size = ins.tellg();
  if (!parse_header(ins, fstdd_size, md_json_header_)) { return false; }
  ins.close();

  key_size_ = md_json_header_["meta"]["Record"];
  block_size_ = md_json_header_["comp_blocks"]["block_size"];
  bucket_size_ = md_json_header_["hash_index"]["bucket_size"];
  comp_blocks_offset_ = md_json_header_["comp_blocks"]["offset"];
  block_index_offset_ = md_json_header_["block_index"]["offset"];
  hash_bucket_offset_ = md_json_header_["hash_buckets"]["offset"];
  hash_index_offset_ = md_json_header_["hash_index"]["offset"];
  keys_offset_ = md_json_header_["keys"]["offset"];

  for (auto &idx : md_json_header_["hash_index"]["dup_idxes"]) {
    dup_idxes_.insert(idx.get<size_t>());
  }
  return true;
}

bool FstddReader::contains(const std::string &key) const {
  ValueBlockPosIndex vbp_idx;
  if (!read_hash_index(fstdd_path_, key, vbp_idx, dup_idxes_, bucket_size_,
                       hash_bucket_offset_, hash_index_offset_)) {
    return false;
  }
  return true;
}

bool FstddReader::extract_all_key(std::vector<std::string> &all_keys) const {
  std::vector<char> keys_buff;
  if (!decompress(fstdd_path_, "keys", md_json_header_, keys_buff)) {
    return false;
  }
  std::vector<std::string> temp_keys;
  temp_keys.reserve(key_size_);
  size_t start_idx = 0;
  for (size_t i = 0; i < keys_buff.size(); ++i) {
    if (keys_buff[i] == '\0') {
      string key(keys_buff.data() + start_idx, i - start_idx);
      temp_keys.emplace_back(std::move(key));
      start_idx = i + 1;
    }
  }
  all_keys.swap(temp_keys);
  return true;
}

bool FstddReader::check_dst_dir(const std::string &dst_dir_str) const {
  fs::path dst_dir(dst_dir_str);
  if (fs::exists(dst_dir)) {
    if (!fs::is_directory(dst_dir)) {
      LOG_ERROR("Destination must be a directory: {}", dst_dir_str);
      return false;
    }
  } else {
    if (!fs::create_directories(dst_dir)) {
      LOG_ERROR("Cannot create the destination directory: {}", dst_dir_str);
      return false;
    }
  }
  return true;
}

bool FstddReader::extract_all(const std::string &dst_dir_str) const {
  if (!check_dst_dir(dst_dir_str)) { return false; }
  std::vector<char> keys_buff;
  if (!decompress(fstdd_path_, "keys", md_json_header_, keys_buff)) {
    return false;
  }
  DyBlockProgBars dynamic_bars;
  auto refresh_bar = dynamic_bars.push_back(key_size_, "Decompressing files:");
  size_t start_idx = 0, file_idx = 0;
  for (size_t i = 0; i < keys_buff.size(); ++i) {
    if (keys_buff[i] == '\0') {
      string key(keys_buff.data() + start_idx, i - start_idx);
      fs::path output_path = fs::path(dst_dir_str) / fs::path(key);
      if (!extract_impl(key, output_path.string())) { return false; }
      refresh_bar(file_idx);
      file_idx += 1;
      start_idx = i + 1;
    }
  }
  return true;
}

bool FstddReader::extract(const std::string &key,
                          const std::string &dst_dir_str) const {
  if (!check_dst_dir(dst_dir_str)) { return false; }
  fs::path output_path = fs::path(dst_dir_str) / fs::path(key);
  return extract_impl(key, output_path.string());
}

bool FstddReader::extract_impl(const string &key,
                               const string &output_path) const {
  std::ifstream ins(fstdd_path_, std::ios::binary);
  if (!ins) {
    LOG_ERROR("Cannot open the file: {}", fstdd_path_);
    return false;
  }

  ValueBlockPosIndex vbp_idx;
  if (!read_hash_index(ins, key, vbp_idx, dup_idxes_, bucket_size_,
                       hash_bucket_offset_, hash_index_offset_)) {
    LOG_ERROR("Not found the key: [{}] in {}", key, fstdd_path_);
    return false;
  }
  uint32_t start_index = vbp_idx.get_start_index();
  uint32_t start_offset = vbp_idx.get_start_offset();
  uint32_t end_index = vbp_idx.get_end_index();
  uint32_t end_offset = vbp_idx.get_end_offset();

  vector<uint64_t> block_indexes(end_index - start_index + 1);
  ins.seekg(start_index * sizeof(uint64_t) + block_index_offset_);
  ins.read(reinterpret_cast<char *>(block_indexes.data()),
           sizeof(uint64_t) * block_indexes.size());

  vector<char> comp_blocks;
  size_t step = 4;
  fs::path parent_path = fs::path(output_path).parent_path();
  if (!fs::exists(parent_path)) {
    if (!fs::create_directories(parent_path)) {
      LOG_ERROR("Cannot create directories: {}", parent_path.string());
      return false;
    }
  }
  ofstream out(output_path, std::ios::binary);
  if (!out) {
    LOG_ERROR("Cannot open the file: {}", output_path);
    return false;
  }

  std::vector<char> decomp_buf(block_size_);
  for (size_t block_idx = 0; block_idx < block_indexes.size(); ++block_idx) {
    size_t idx = block_idx;
    uint64_t start_block_offset = block_indexes[block_idx] >> 24;
    if (block_idx + step >= block_indexes.size()) {
      step = block_indexes.size() - 1 - block_idx;
    }
    block_idx += step;
    uint64_t end_block_offset = block_indexes[block_idx] >> 24;
    uint64_t end_block_size = block_indexes[block_idx] & 0xFFFFFF;
    uint64_t total_block_size =
        end_block_offset - start_block_offset + end_block_size;
    comp_blocks.resize(total_block_size);
    ins.seekg(start_block_offset + comp_blocks_offset_);
    ins.read(comp_blocks.data(), comp_blocks.size());
    size_t buf_offset = 0;
    for (; idx <= block_idx; ++idx) {
      size_t comp_block_size = block_indexes[idx] & 0xFFFFFF;
      size_t decomp_size =
          ZSTD_decompress(decomp_buf.data(), block_size_,
                          comp_blocks.data() + buf_offset, comp_block_size);
      if (ZSTD_isError(decomp_size)) {
        LOG_ERROR("Decompress failed: {}", ZSTD_getErrorName(decomp_size));
        return false;
      }
      size_t offset = 0;
      size_t size = decomp_size;
      if (idx == 0) {
        offset = start_offset;
        size = decomp_size - start_offset;
      }
      if (idx == block_indexes.size() - 1) { size = end_offset - offset; }
      out.write(decomp_buf.data() + offset, size);
      buf_offset += comp_block_size;
    }
  }
  return true;
}

} // namespace fstd