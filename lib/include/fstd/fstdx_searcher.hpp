#pragma once

#include <fstream>
#include <nlohmann/json.hpp>

#include <fstd/compress_dx.hpp>
#include <fstd/fstdx_reader.hpp>
#include <fstd/fstdx_writer.hpp>
#include <fstd/logger.hpp>

using namespace std;
using json = nlohmann::json;

namespace fstd {
using FstdxSearcher = FstdxReader;

// class FstdxSearcher {
// public:
//   FstdxSearcher(const std::string &fstdx_path, bool &is_valid)
//       : fstdx_path(fstdx_path), ddict(nullptr) {
//     if (!parse_fstdx(fstdx_path)) {
//       is_valid = false;
//       return;
//     }
//     is_valid = true;
//   }
//   ~FstdxSearcher() { ZSTD_freeDDict(ddict); }

//   const json &get_meta() const { return mx_json_header["meta"]; }

//   bool exact_match_search(std::string_view word,
//                           std::vector<std::string> &result) const {
//     uint64_t index_res = 0;
//     if (!fst_map_searcher.exact_match_search(word, index_res)) { return false; }
//     uint32_t index = 0;
//     uint32_t duplicate = 0;
//     if (index_res < UINT32_MAX) {
//       index = static_cast<uint32_t>(index_res);
//     } else {
//       index = static_cast<uint32_t>(index_res & 0xFFFFFFFF);
//       duplicate = static_cast<uint32_t>(index_res >> 32);
//     }
//     LOG_INFO("index: {}, duplicate: {}", index, duplicate);
//     std::vector<std::string> tmp_result;
//     for (uint32_t i = 0; i <= duplicate; ++i) {
//       std::string text = dx_compressor.readTextByIndex(
//           index + i, ddict, block_indexes, entry_indexes, fstdx_path,
//           comp_text_offset);
//       tmp_result.emplace_back(text);
//     }
//     result.swap(tmp_result);
//     return true;
//   }

//   std::vector<std::pair<std::string, uint64_t>>
//   common_prefix_search(std::string_view word) const {
//     return fst_map_searcher.common_prefix_search(word);
//   }

//   size_t
//   longest_common_prefix_search(std::string_view word,
//                                std::pair<std::string, uint64_t> &result) const {
//     return fst_map_searcher.longest_common_prefix_search(word, result);
//   }

//   std::vector<std::pair<std::string, uint64_t>>
//   predictive_search(std::string_view word) const {
//     return fst_map_searcher.predictive_search(word);
//   }

//   std::vector<std::pair<std::string, uint64_t>>
//   edit_distance_search(std::string_view word, size_t edit_distance) const {
//     return fst_map_searcher.edit_distance_search(word, edit_distance);
//   }

//   std::pair<std::vector<std::pair<std::string, uint64_t>>, std::string>
//   regex_search(std::string_view pattern) const {
//     return fst_map_searcher.regex_search(pattern);
//   }

//   std::vector<std::tuple<double, std::string, uint64_t>>
//   spellcheck_word(std::string_view word, const size_t n = 10) const {
//     return fst_map_searcher.spellcheck_word(word, n);
//   }

//   std::vector<std::pair<std::string, uint64_t>> enumerate() const {
//     return fst_map_searcher.enumerate();
//   }

// private:
//   bool parse_fstdx(const std::string &fstdx_path) {
//     std::ifstream ins(fstdx_path, std::ios::binary | std::ios::ate);
//     if (!ins) {
//       LOG_ERROR("Cannot open the file: {}", fstdx_path);
//       return false;
//     }
//     size_t fstdx_size = ins.tellg();
//     size_t record_size = sizeof(MxHeaderSizeRecord);
//     if (fstdx_size < record_size) {
//       LOG_ERROR("It is not a valid fstdx file: {}", fstdx_path);
//       return false;
//     }
//     LOG_INFO("fstdx_size:{}", fstdx_size);
//     ins.seekg(-record_size, std::ios::end);
//     MxHeaderSizeRecord header_size_record;
//     LOG_INFO("record_size:{}", record_size);
//     ins.read(reinterpret_cast<char *>(&header_size_record), record_size);
//     if (fstdx_size < header_size_record.compressed_size) {
//       LOG_ERROR("It is not a valid fstdx file: {}", fstdx_path);
//       return false;
//     }

//     LOG_INFO("header_size_record: original_size:{}, compressed_size:{}",
//              header_size_record.original_size,
//              header_size_record.compressed_size);

//     vector<char> header_compressed_byte(header_size_record.compressed_size);
//     ins.seekg(-(record_size + header_size_record.compressed_size),
//               std::ios::end);
//     ins.read(const_cast<char *>(header_compressed_byte.data()),
//              header_size_record.compressed_size);
//     std::vector<char> header_json_raw_str;
//     dx_compressor.decompressToBuffer(
//         header_compressed_byte.data(), header_compressed_byte.size(),
//         header_size_record.original_size, header_json_raw_str);
//     // MxJsonHeader mx_json_header;
//     try {
//       mx_json_header = json::parse(string(header_json_raw_str.data()));
//       LOG_INFO("{}", mx_json_header.dump());
//     } catch (const json::exception &e) {
//       LOG_ERROR("解析 header 失败: {}", e.what());
//       return false;
//     } catch (...) {
//       LOG_ERROR("解析 header 失败: 未知异常");
//       return false;
//     }

//     std::vector<char> key_fst_byte_code;
//     if (!decompress(ins, "key_fst", mx_json_header, key_fst_byte_code)) {
//       return false;
//     }
//     fst_map_searcher = FstMapSearcher<uint64_t>(key_fst_byte_code);

//     std::vector<char> dictBuffer;
//     if (!decompress(ins, "comp_dict", mx_json_header, dictBuffer)) {
//       return false;
//     }
//     ddict = ZSTD_createDDict(dictBuffer.data(), dictBuffer.size());

//     if (!decompress(ins, "block_indexes", mx_json_header, block_indexes)) {
//       return false;
//     }

//     if (!decompress(ins, "entry_indexes", mx_json_header, entry_indexes)) {
//       return false;
//     }

//     ins.close();

//     comp_text_offset = mx_json_header["comp_blocks"]["offset"];

//     return true;
//   }

//   template <typename T>
//   bool decompress(istream &ins, const std::string &block_name,
//                   const MxJsonHeader &mx_json_header, std::vector<T> &con) {
//     const json &json_block = mx_json_header[block_name];
//     int compress_level = json_block["compress_level"];
//     uint64_t offset = json_block["offset"];
//     uint64_t original_size = json_block["original_size"];
//     std::vector<T> tmp_con;
//     tmp_con.resize(original_size / sizeof(T));
//     if (compress_level == 0) {
//       ins.seekg(offset);
//       ins.read(reinterpret_cast<char *>(tmp_con.data()), tmp_con.size());
//     } else {
//       uint64_t compressed_size = json_block["compressed_size"];
//       vector<char> compressed_block(compressed_size);
//       ins.seekg(offset);
//       ins.read(compressed_block.data(), compressed_block.size());
//       std::vector<char> dst_buff(original_size);
//       bool res = dx_compressor.decompressToBuffer(compressed_block.data(),
//                                                   compressed_block.size(),
//                                                   original_size, dst_buff);
//       if (!res) { return false; }
//       memcpy(tmp_con.data(), dst_buff.data(), dst_buff.size());
//     }
//     con.swap(tmp_con);
//     return true;
//   }

// private:
//   const std::string &fstdx_path;
//   MxJsonHeader mx_json_header;
//   FstMapSearcher<uint64_t> fst_map_searcher;
//   Dxcompressor dx_compressor;
//   ZSTD_DDict *ddict;
//   std::vector<BlockIndex> block_indexes;
//   std::vector<EntryIndex> entry_indexes;
//   uint64_t comp_text_offset;
// };
} // namespace fstd