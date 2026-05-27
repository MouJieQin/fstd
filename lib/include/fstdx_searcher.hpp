#pragma once

#include "compress_dx.hpp"
#include "fstdx_writer.hpp"
#include "logger.hpp"
#include <fstream>

namespace fstd {

class FstdxSearcher {
public:
  FstdxSearcher(const std::string &fstdx_path, bool &is_valid)
      : fstdx_path(fstdx_path) {
    if (!parse_fstdx(fstdx_path)) {
      is_valid = false;
      return;
    }
    is_valid = true;
  }

  bool exact_match_search(std::string_view word,
                          std::vector<std::string> &result) {
    uint64_t index_res = 0;
    if (!fst_map_searcher.exact_match_search(word, index_res)) { return false; }
    uint32_t index = 0;
    uint32_t duplicate = 0;
    if (index_res < UINT32_MAX) {
      index = static_cast<uint32_t>(index_res);
    } else {
      index = static_cast<uint32_t>(index_res & 0xFFFFFFFF);
      duplicate = static_cast<uint32_t>(index_res >> 32);
    }
    LOG_INFO("index: {}, duplicate: {}", index, duplicate);
    std::vector<std::string> tmp_result;
    for (uint32_t i = 0; i <= duplicate; ++i) {
      std::string text = dx_compressor.readTextByIndex(
          index + i, dictBuffer, block_indexes, entry_indexes, fstdx_path,
          comp_text_offset);
      tmp_result.emplace_back(text);
    }
    result.swap(tmp_result);
    return true;
  }

private:
  bool parse_fstdx(const std::string &fstdx_path) {
    std::ifstream ins(fstdx_path, std::ios::binary | std::ios::ate);
    if (!ins) {
      LOG_ERROR("Cannot open the file: {}", fstdx_path);
      return false;
    }
    size_t fstdx_size = ins.tellg();
    uint64_t mx_head_size = sizeof(MxHeader);
    if (fstdx_size < mx_head_size) {
      LOG_ERROR("It is not a valid fstdx file: {}", fstdx_path);
      return false;
    }
    ins.seekg(0);
    MxHeader mx_header;
    ins.read(reinterpret_cast<char *>(&mx_header), mx_head_size);
    if (fstdx_size < mx_header.get_fstdx_size()) {
      LOG_ERROR("fstdx file size: {}", fstdx_path);

      LOG_ERROR("It is not a valid fstdx file: {}", fstdx_path);
      return false;
    }

    std::vector<char> key_fst_byte_code(mx_header.key_fst_size);
    ins.seekg(mx_head_size);
    ins.read(key_fst_byte_code.data(), key_fst_byte_code.size());
    fst_map_searcher = FstMapSearcher<uint64_t>(key_fst_byte_code);

    dictBuffer.resize(mx_header.dict_size);
    ins.seekg(mx_head_size + mx_header.key_fst_size);
    ins.read(dictBuffer.data(), dictBuffer.size());

    block_indexes.resize(mx_header.block_index_size / sizeof(BlockIndex));
    ins.seekg(mx_head_size + mx_header.key_fst_size + mx_header.dict_size);
    ins.read(reinterpret_cast<char *>(block_indexes.data()),
             mx_header.block_index_size);

    entry_indexes.resize(mx_header.entry_index_size / sizeof(EntryIndex));
    ins.seekg(mx_head_size + mx_header.key_fst_size + mx_header.dict_size +
              mx_header.block_index_size);
    ins.read(reinterpret_cast<char *>(entry_indexes.data()),
             mx_header.entry_index_size);

    ins.close();
    comp_text_offset = mx_header.get_fstdx_size() - mx_header.comp_size;
    return true;
  }

private:
  const std::string &fstdx_path;
  FstMapSearcher<uint64_t> fst_map_searcher;
  Dxcompressor dx_compressor;
  std::vector<char> dictBuffer;
  std::vector<BlockIndex> block_indexes;
  std::vector<EntryIndex> entry_indexes;
  uint64_t comp_text_offset;
};
} // namespace fstd