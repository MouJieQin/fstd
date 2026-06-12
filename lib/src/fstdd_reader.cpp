#include <fstd/common.h>
#include <fstd/fstdd_compressor.h>
#include <fstd/fstdd_reader.h>
#include <fstd/hash_index.h>
#include <fstd/logger.h>

namespace fstd {

using namespace std;
using json = nlohmann::json;

FstddReader::FstddReader(const std::string &fstdd_path, bool &is_valid)
    : fstdd_path_(fstdd_path) {
  if (!parse_fstdd(fstdd_path_)) {
    is_valid = false;
    return;
  }
  is_valid = true;
}

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

  bucket_size_ = md_json_header_["hash_index"]["bucket_size"];
  comp_blocks_offset_ = md_json_header_["comp_blocks"]["offset"];
  block_index_offset_ = md_json_header_["block_index"]["offset"];
  hash_bucket_offset_ = md_json_header_["hash_buckets"]["offset"];
  hash_index_offset_ = md_json_header_["hash_index"]["offset"];
  keys_offset_ = md_json_header_["keys"]["offset"];

  for (auto idx : md_json_header_["hash_index"]["dup_idxes"]) {
    dup_idxes_.insert(static_cast<size_t>(idx));
  }
  return true;
}

bool FstddReader::extract(const std::string &key, const std::string &dst_dir) {
  return extract_impl(key, dst_dir);
}

bool FstddReader::extract_impl(const string &key, const string &dst_dir) {
  std::ifstream ins(fstdd_path_, std::ios::binary);
  if (!ins) {
    LOG_ERROR("Cannot open the file: {}", fstdd_path_);
    return false;
  }

  ValueBlockPosIndex vbp_idx;
  if (!read_hash_index(ins, key, vbp_idx, dup_idxes_, bucket_size_,
                       hash_bucket_offset_, hash_index_offset_)) {
    cout << "Not found the key: " << key << endl;
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

  string rel_path(key);
  std::replace(rel_path.begin(), rel_path.end(), '/', '_');
  const string file_path = dst_dir + "/" + rel_path;

  ofstream out(file_path, std::ios::binary);
  if (!out) {
    std::cout << "open error" << std::endl;
    return false;
  }
  std::vector<char> decomp_buf(zstd_block_size);
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
      size_t block_size = block_indexes[idx] & 0xFFFFFF;
      size_t decomp_size =
          ZSTD_decompress(decomp_buf.data(), zstd_block_size,
                          comp_blocks.data() + buf_offset, block_size);
      if (ZSTD_isError(decomp_size)) {
        cerr << "解压失败: " << ZSTD_getErrorName(decomp_size) << endl;
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
      buf_offset += block_size;
    }
  }
  return true;
}

} // namespace fstd