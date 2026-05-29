#pragma once
#include <fstream>
#include <nlohmann/json.hpp>
#include <sstream>

#include "compress_dx.hpp"
#include "fstlib_wrapper.h"
#include "logger.hpp"

using namespace fst;
using namespace std;
using json = nlohmann::json;

namespace fstd {

struct MxHeaderSizeRecord {
  MxHeaderSizeRecord() = default;
  MxHeaderSizeRecord(uint32_t original_size, uint32_t compressed_size)
      : original_size(original_size), compressed_size(compressed_size) {}
  uint32_t original_size;
  uint32_t compressed_size;
};

std::string get_current_date() {
  std::time_t now = std::time(nullptr);

  std::tm local_tm = *std::localtime(&now);

  char buf[32];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", local_tm.tm_year + 1900,
                local_tm.tm_mon + 1, local_tm.tm_mday);

  return std::string(buf);
}

using MxJsonHeader = json;

class FstdxWriter {
public:
  FstdxWriter() = default;

  static const json meta_default;

  bool handle_meta(const json &meta, MxJsonHeader &header) {
    if (!meta.is_object()) {
      LOG_ERROR("Meta is not an object.");
      return false;
    }

    json &h_meta = header["meta"];
    // 遍历 meta_default
    for (const auto &item : FstdxWriter::meta_default) {
      // item 是数组里的每个小对象，比如 {"Version":""}

      // 遍历这个小对象里的 唯一一个 键值对
      for (const auto &[key, value] : item.items()) {
        if (!meta.contains(key)) {
          h_meta[key] = value;
        } else if (value.is_string()) {
          if (meta[key].is_string()) {
            h_meta[key] = meta[key];
          } else {
            LOG_ERROR("Meta {} is not a string.", key);
            return false;
          }
        } else if (value.is_boolean()) {
          if (meta[key].is_boolean()) {
            h_meta[key] = meta[key];
          } else {
            LOG_ERROR("Meta {} is not a boolean.", key);
            return false;
          }
        } else if (value.is_number()) {
          if (meta[key].is_number()) {
            h_meta[key] = meta[key];
          } else {
            LOG_ERROR("Meta {} is not a number.", key);
            return false;
          }
        } else {
          LOG_ERROR(
              "Internal Error: Meta {} is not a string, boolean, or number.",
              key);
          return false;
        }
      }
    }

    return true;
  }

  int compile_fstdmx(const std::string &input_file,
                     const std::string &output_file,
                     const std::string &delimiter, const json &meta,
                     uint16_t block_size_kb, uint8_t compress_level,
                     uint16_t zstd_dict_size_kb, bool opt_sorted,
                     bool opt_verbose) {
    ifstream fin(input_file);
    if (!fin) {
      LOG_ERROR("Failed to open file {} for reading.", input_file);
      return 1;
    }

    ofstream fout(output_file, ios_base::binary);
    if (!fout) {
      LOG_ERROR("Failed to open file {} for writing.", output_file);
      return 1;
    }

    vector<string> keys;
    vector<string> values;
    MxJsonHeader header;
    if (!handle_meta(meta, header)) { return 5; }
    LOG_INFO("handle_meta success.");
    if (!load_file(fin, keys, values, delimiter)) {
      fin.close();
      return 1;
    }
    header["meta"]["Record"] = keys.size();
    header["meta"]["Stripkey"] = true;
    header["meta"]["CompressionLevel"] = compress_level;
    LOG_INFO("Loaded {} keys and {} values.", keys.size(), values.size());
    if (!opt_sorted) { sort_keys_values(keys, values); }
    LOG_INFO("Sorted {} keys and {} values.", keys.size(), values.size());

    ofstream sorted_keys_fout("sorted_keys.txt", ios_base::out);
    if (!sorted_keys_fout) { return 1; }
    for (const auto &key : keys) {
      sorted_keys_fout << key << "\n";
    }
    sorted_keys_fout.close();

    vector<pair<string, uint64_t>> input;
    make_output(keys, input);

    {
      // 释放keys内存
      vector<string> tmp;
      keys.swap(tmp);
    }

    ofstream input_fout("input.txt", ios_base::out);
    if (!input_fout) { return 1; }
    for (const auto &p : input) {
      input_fout << '|' << p.first << "|" << p.first.size() << ": " << p.second
                 << "\n";
    }
    input_fout.close();

    ofstream value_fout("values.txt", ios_base::out);
    if (!value_fout) { return 1; }
    for (const auto &value : values) {
      std::string line = "";
      for (auto c : value) {
        if (c != '\n') { line += c; }
      }
      value_fout << line << "\n";
    }
    value_fout.close();

    ostringstream oss_key_fst_out(ios_base::binary);
    if (!compile_fst(input, oss_key_fst_out, true, opt_verbose)) { return 2; }
    {
      // 释放input内存
      vector<pair<string, uint64_t>> tmp;
      input.swap(tmp);
    }

    Dxcompressor compressor;
    std::vector<char> comp_key_fst_dst;
    bool comp_res = false;
    {
      comp_res = compressor.compressToBuffer(oss_key_fst_out.str(),
                                             oss_key_fst_out.str().size(),
                                             comp_key_fst_dst, compress_level);
      if (!comp_res) { return 4; }
      header["key_fst"]["compress_level"] = compress_level;
      header["key_fst"]["original_size"] = oss_key_fst_out.str().size();
      header["key_fst"]["compressed_size"] = comp_key_fst_dst.size();
      std::ofstream key_fst_fout("key_fst.fst", ios_base::binary);
      key_fst_fout << oss_key_fst_out.str();
      ostringstream tmp(ios_base::binary);
      oss_key_fst_out.swap(tmp);
    }

    std::ostringstream dictOut(ios_base::binary);
    std::ostringstream blockIdxOut(ios_base::binary);
    std::ostringstream entryIdxOut(ios_base::binary);
    std::ostringstream compOut(ios_base::binary);
    LOG_INFO("Compressing values...");
    if (!compressor.compressTextToStream(
            values, dictOut, blockIdxOut, entryIdxOut, compOut,
            block_size_kb * 1024, zstd_dict_size_kb * 1024, compress_level)) {
      return 3;
    }

    std::ofstream dict_fout("zstd_dict.bin", ios_base::binary);
    dict_fout << dictOut.str();
    header["comp_dict"]["compress_level"] = 0;
    header["comp_dict"]["original_size"] = dictOut.str().size();

    std::vector<char> comp_block_index_dst;
    {
      comp_res = compressor.compressToBuffer(
          blockIdxOut.str(), blockIdxOut.str().size(), comp_block_index_dst,
          compress_level);
      if (!comp_res) { return 4; }
      header["block_indexes"]["compress_level"] = compress_level;
      header["block_indexes"]["compressed_size"] = comp_block_index_dst.size();
      header["block_indexes"]["original_size"] = blockIdxOut.str().size();
      std::ofstream block_index_fout("block.idx", ios_base::binary);
      block_index_fout << blockIdxOut.str();
      ostringstream tmp(ios_base::binary);
      blockIdxOut.swap(tmp);
    }

    std::vector<char> comp_entry_index_dst;
    {
      comp_res = compressor.compressToBuffer(
          entryIdxOut.str(), entryIdxOut.str().size(), comp_entry_index_dst,
          compress_level);
      if (!comp_res) { return 4; }
      header["entry_indexes"]["compress_level"] = compress_level;
      header["entry_indexes"]["original_size"] = entryIdxOut.str().size();
      header["entry_indexes"]["compressed_size"] = comp_entry_index_dst.size();
      std::ofstream entry_index_fout("entry.idx", ios_base::binary);
      entry_index_fout << entryIdxOut.str();
      ostringstream tmp(ios_base::binary);
      entryIdxOut.swap(tmp);
    }

    header["comp_blocks"]["compress_level"] = compress_level;
    std::ofstream comp_fout("dict.zst", ios_base::binary);
    comp_fout << compOut.str();
    comp_fout.flush();
    comp_fout.close();

    // fout << oss_key_fst_out.str();
    fout.write(comp_key_fst_dst.data(), comp_key_fst_dst.size());
    header["key_fst"]["offset"] = 0;

    fout << dictOut.str();
    header["comp_dict"]["offset"] =
        static_cast<size_t>(header["key_fst"]["offset"]) +
        comp_key_fst_dst.size();

    // fout << blockIdxOut.str();
    fout.write(comp_block_index_dst.data(), comp_block_index_dst.size());
    header["block_indexes"]["offset"] =
        static_cast<size_t>(header["comp_dict"]["offset"]) +
        dictOut.str().size();

    fout.write(comp_entry_index_dst.data(), comp_entry_index_dst.size());
    header["entry_indexes"]["offset"] =
        static_cast<size_t>(header["block_indexes"]["offset"]) +
        comp_block_index_dst.size();

    fout << compOut.str();
    header["comp_blocks"]["offset"] =
        static_cast<size_t>(header["entry_indexes"]["offset"]) +
        comp_entry_index_dst.size();

    header["meta"]["Creationdate"] = get_current_date();
    std::vector<char> comp_header_dst;
    std::string header_str = header.dump();
    {
      comp_res =
          compressor.compressToBuffer(header_str.c_str(), header_str.size(),
                                      comp_header_dst, compress_level);
      if (!comp_res) { return 4; }
    }
    fout.write(comp_header_dst.data(), comp_header_dst.size());

    std::ofstream header_fout("header.zst", ios_base::binary);
    header_fout << comp_header_dst.data();

    MxHeaderSizeRecord header_size_record(header_str.size(),
                                          comp_header_dst.size());
    LOG_INFO("{}", header_str);
    LOG_INFO("{},{}", header_size_record.original_size,
             header_size_record.compressed_size);
    fout.write(reinterpret_cast<const char *>(&header_size_record),
               sizeof(MxHeaderSizeRecord));
    return 0;
  }

private:
  std::string trim_whitespace(const std::string &s) {
    size_t start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";

    size_t end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
  }

  bool parse_raw_txt(std::vector<std::string> &raw_lines,
                     std::vector<size_t> &delimiter_indices,
                     std::vector<std::string> &keys,
                     std::vector<std::string> &values) {
    if (delimiter_indices.empty()) {
      LOG_ERROR("Not found valid key and value lines. [not found delimiter]");
      return false;
    }
    if (delimiter_indices.front() < 2) {
      LOG_ERROR("Not found valid key and value lines between the start of file "
                "and the line {}.",
                delimiter_indices.front() + 1);
      return false;
    }
    for (size_t i = 1; i < delimiter_indices.size(); i++) {
      if (delimiter_indices[i] - delimiter_indices[i - 1] < 2) {
        LOG_ERROR("Not found valid key and value lines between the line {} and "
                  "the line {}.",
                  delimiter_indices[i - 1] + 1, delimiter_indices[i] + 1);
        return false;
      }
    }
    if (delimiter_indices.back() + 1 < raw_lines.size()) {
      LOG_WARN(
          "The raw text file not ends with a delimiter. the content after the "
          "last delimiter [line: {}] will be ignored.",
          delimiter_indices.back() + 1);
    }

    vector<string> keys_temp;
    keys_temp.reserve(delimiter_indices.size());
    // handle the first key
    keys_temp.emplace_back(trim_whitespace(raw_lines[0]));
    for (size_t i = 0; i < delimiter_indices.size() - 1; i++) {
      keys_temp.emplace_back(
          trim_whitespace(raw_lines[delimiter_indices[i] + 1]));
    }

    vector<string> values_temp;
    values_temp.reserve(delimiter_indices.size());
    string cur_value("");
    // handle the first value
    for (size_t i = 1; i < delimiter_indices.front(); i++) {
      cur_value += raw_lines[i] + "\n";
    }
    // remove the last '\n'
    cur_value.pop_back();
    values_temp.emplace_back(std::move(cur_value));
    for (size_t i = 0; i < delimiter_indices.size() - 1; i++) {
      cur_value = "";
      for (size_t j = delimiter_indices[i] + 2; j < delimiter_indices[i + 1];
           j++) {
        cur_value += std::move(raw_lines[j]) + "\n";
      }
      cur_value.pop_back();
      values_temp.emplace_back(std::move(cur_value));
    }
    keys.swap(keys_temp);
    values.swap(values_temp);
    return true;
  }

  bool load_file(ifstream &fin, std::vector<std::string> &keys,
                 std::vector<std::string> &values,
                 const std::string &delimiter) {
    LOG_INFO("Loading and parsing raw text file...");
    std::string line;
    vector<string> raw_lines;
    raw_lines.reserve(100000);
    vector<size_t> delimiter_indices;
    size_t index_count = 0;
    while (getline(fin, line)) {
      string trimmed_line = trim_whitespace(line);
      if (trimmed_line == delimiter) {
        delimiter_indices.emplace_back(index_count);
      }
      raw_lines.emplace_back(std::move(line));
      index_count += 1;
    }

    return parse_raw_txt(raw_lines, delimiter_indices, keys, values);
  }

  void sort_keys_values(std::vector<std::string> &keys,
                        std::vector<std::string> &values) {
    LOG_INFO("Sorting {} keys and values...", keys.size());
    vector<size_t> indices(keys.size());
    iota(indices.begin(), indices.end(), 0);
    sort(indices.begin(), indices.end(),
         [&](size_t i, size_t j) { return keys[i] < keys[j]; });
    vector<string> temp_keys;
    temp_keys.reserve(keys.size());
    vector<string> temp_values;
    temp_values.reserve(values.size());
    for (size_t i : indices) {
      temp_keys.emplace_back(std::move(keys[i]));
      temp_values.emplace_back(std::move(values[i]));
    }
    keys.swap(temp_keys);
    values.swap(temp_values);
  }

  uint64_t get_output(uint32_t index, uint32_t duplicate_count) {
    uint64_t duplicate_mask = static_cast<uint64_t>(duplicate_count) << 32;

    return duplicate_mask | static_cast<uint64_t>(index);
  }

  void make_output(std::vector<std::string> &sorted_keys,
                   std::vector<std::pair<std::string, uint64_t>> &input) {
    if (sorted_keys.empty()) { return; }
    LOG_INFO("Encoding duplicate keys for compiling FST...");
    std::vector<std::pair<std::string, uint64_t>> temp_input;
    temp_input.reserve(input.size());
    uint32_t index = 0;
    uint32_t duplicate_count = 0;
    uint32_t total_duplicate_count = 0;
    for (uint32_t i = 1; i < sorted_keys.size(); i++) {
      if (sorted_keys[index].size() == sorted_keys[i].size() &&
          sorted_keys[index] == sorted_keys[i]) {
        duplicate_count++;
      } else {
        if (duplicate_count > 0) { total_duplicate_count += 1; }
        uint64_t output = get_output(index, duplicate_count);
        temp_input.emplace_back(std::move(sorted_keys[index]), output);
        duplicate_count = 0;
        index = i;
      }
    }
    // 处理最后一个key
    if (duplicate_count > 0) { total_duplicate_count += 1; }
    LOG_INFO("Total duplicate keys: {}", total_duplicate_count);
    uint64_t output = get_output(index, duplicate_count);
    temp_input.emplace_back(std::move(sorted_keys[index]), output);
    input.swap(temp_input);
  }

  bool compile_fst(std::vector<std::pair<std::string, uint64_t>> &input,
                   std::ostringstream &oss_out, bool opt_sorted,
                   bool opt_verbose) {
    LOG_INFO("Compiling FST for {} keys...", input.size());
    return fstd::compile_fst(input, oss_out, opt_sorted, opt_verbose);
  }
};

const json FstdxWriter::meta_default =
    json::array({{{"Version", ""}},
                 {{"Record", 0}},
                 {{"Format", "Html"}},
                 {{"Keycasesensitive", false}},
                 {{"Stripkey", true}},
                 {{"Description", ""}},
                 {{"Title", ""}},
                 {{"Encoding", "UTF-8"}},
                 {{"Creationdate", ""}},
                 {{"CompressionLevel", 5}},
                 {{"Left2Right", true}},
                 {{"Stylesheet", ""}}});
} // namespace fstd