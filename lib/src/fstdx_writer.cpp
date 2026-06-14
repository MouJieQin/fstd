#include <fstream>

#include <fstd/common.h>
#include <fstd/fstdx_compressor.h>
#include <fstd/fstdx_writer.h>
#include <fstd/hash_index.h>
#include <fstd/logger.h>
#include <fstd/thread_pool.h>
#include <indicators/block_progress_bar.hpp>
#include <indicators/cursor_control.hpp>
#include <indicators/dynamic_progress.hpp>

using namespace fst;
using namespace std;
using namespace indicators;
using json = nlohmann::json;

namespace fstd {

int FstdxWriter::compile_fstdx(const std::string &output_file,
                               std::vector<std::string> &keys,
                               std::vector<std::string> &values,
                               const json &meta, uint16_t block_size_kb,
                               uint8_t compress_level,
                               uint16_t zstd_dict_size_kb, size_t worker_num,
                               bool opt_sorted, bool opt_verbose) {
  ofstream fout(output_file, ios_base::binary);
  if (!fout) {
    LOG_ERROR("Failed to open file {} for writing.", output_file);
    return 1;
  }
  return compile_fstdx(fout, keys, values, meta, block_size_kb, compress_level,
                       zstd_dict_size_kb, worker_num, opt_sorted, opt_verbose);
}

int FstdxWriter::compile_fstdx(const std::string &output_file,
                               std::vector<std::string> &keys,
                               std::vector<std::string> &values,
                               const std::string &meta_json_str,
                               uint16_t block_size_kb, uint8_t compress_level,
                               uint16_t zstd_dict_size_kb, size_t worker_num,
                               bool opt_sorted, bool opt_verbose) {

  json meta_json;
  try {
    meta_json = json::parse(meta_json_str);
  } catch (const json::exception &e) {
    LOG_ERROR("JSON string {} 格式错误：{}", meta_json_str, e.what());
    return 1;
  } catch (const std::exception &e) {
    LOG_ERROR("JSON string {} 读取错误：{}", meta_json_str, e.what());
    return 1;
  }
  return compile_fstdx(output_file, keys, values, meta_json, block_size_kb,
                       compress_level, zstd_dict_size_kb, worker_num,
                       opt_sorted, opt_verbose);
}

int FstdxWriter::compile_fstdx(const std::string &input_file,
                               const std::string &output_file, const json &meta,
                               uint16_t block_size_kb, uint8_t compress_level,
                               uint16_t zstd_dict_size_kb, size_t worker_num,
                               bool opt_sorted, bool opt_verbose) {
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
  if (!load_file(fin, keys, values)) { return 1; }
  fin.close();
  return compile_fstdx(fout, keys, values, meta, block_size_kb, compress_level,
                       zstd_dict_size_kb, worker_num, opt_sorted, opt_verbose);
}

int FstdxWriter::compile_fstdx(std::ostream &fout,
                               std::vector<std::string> &keys,
                               std::vector<std::string> &values,
                               const json &meta, uint16_t block_size_kb,
                               uint8_t compress_level,
                               uint16_t zstd_dict_size_kb, size_t worker_num,
                               bool opt_sorted, bool opt_verbose) {
  DxJsonHeader header;
  if (!handle_meta(meta, meta_default, header)) { return 5; }
  LOG_INFO("handle meta success.");
  header["meta"]["Record"] = keys.size();
  header["meta"]["Stripkey"] = true;
  header["meta"]["Compressionlevel"] = compress_level;
  LOG_INFO("Loaded {} keys and {} values.", keys.size(), values.size());
  if (!opt_sorted) { sort_keys_values(keys, values); }
  LOG_INFO("Sorted {} keys and {} values.", keys.size(), values.size());

  vector<pair<string, uint64_t>> input;
  make_output(keys, input);
  header["key_fst"]["keys_size"] = input.size();
  {
    // 释放keys内存
    vector<string> tmp;
    keys.swap(tmp);
  }

  // Hide cursor
  show_console_cursor(false);
  DynamicProgress<BlockProgressBar> bars;
  auto build_fst_bar_ptr = std::make_unique<BlockProgressBar>(
      option::BarWidth{80}, option::Start{"|"}, option::End{"|"},
      option::PrefixText{"Compiling key FST:        "},
      option::ShowElapsedTime{true}, option::ShowRemainingTime{true},
      option::ForegroundColor{Color::cyan}, option::ShowPercentage{true},
      option::FontStyles{std::vector<FontStyle>{FontStyle::bold}});
  bars.set_option(option::HideBarWhenComplete{false});
  size_t thread_num = worker_num;
  if (worker_num == 0) { thread_num = get_cpu_core_count(); }
  ThreadPool thread_pool(thread_num);
  vector<shared_ptr<BlockProgressBar>> block_bars;

  for (size_t i = 0; i < thread_num; ++i) {
    block_bars.emplace_back(std::make_shared<BlockProgressBar>(
        option::BarWidth{80}, option::Start{"|"}, option::End{"|"},
        option::PrefixText{"Compressing value blocks: "},
        option::ShowElapsedTime{true}, option::ShowRemainingTime{true},
        option::ForegroundColor{Color::white}, option::ShowPercentage{true},
        option::FontStyles{std::vector<FontStyle>{FontStyle::bold}}));
    bars.push_back(*block_bars.back());
  }

  ostringstream oss_key_fst_out(ios_base::binary);
  ostringstream oss_hash_index_out(ios_base::binary);
  std::set<size_t> dup_hash_idxes;
  size_t bucket_size, bucket_data_size;
  const bool show_progress = is_terminal();
  auto compile_res = thread_pool.enqueue([&]() {
    size_t i = bars.push_back(*build_fst_bar_ptr);
    size_t last_progress = 0;
    auto progress_build_fst = [&](const size_t index) {
      if (show_progress) {
        size_t progress = index * 100 / input.size();
        if (progress > last_progress) {
          bars[i].set_option(option::PostfixText{std::to_string(index) + "/" +
                                                 std::to_string(input.size())});
          bars[i].set_progress(progress);
          last_progress = progress;
        }
        if (index == input.size()) { bars[i].mark_as_completed(); }
      }
    };

    bool res = compile_fst(input, oss_key_fst_out, true, opt_verbose,
                           progress_build_fst);
    if (res) {
      LOG_INFO("FST compiled.");
    } else {
      LOG_ERROR("Compile FST failed.");
    }
    return res;
  });

  FstdxCompressor compressor;
  bool comp_res = false;

  LOG_INFO("Compressing values...");
  if (!compressor.compress_texts_to_stream(
          fout, values, header, zstd_dict_size_kb * 1024, block_size_kb * 1024,
          compress_level, thread_pool, bars)) {
    return 3;
  }
  // Show cursor
  show_console_cursor(true);

  auto write_hash_res = thread_pool.enqueue([&]() {
    auto res_p = write_hash_index(oss_hash_index_out, input, dup_hash_idxes);
    bucket_size = res_p.first;
    bucket_data_size = res_p.second;
  });
  write_hash_res.get();

  if (!compile_res.get()) { return 2; }

  const string hash_data(oss_hash_index_out.str());
  fout << hash_data;
  header["hash_buckets"]["offset"] =
      static_cast<size_t>(header["entry_indexes"]["offset"]) +
      input.size() * sizeof(EntryIndex);
  header["hash_index"]["offset"] =
      static_cast<size_t>(header["hash_buckets"]["offset"]) + bucket_data_size;
  header["hash_index"]["dup_idxes"] =
      vector<size_t>(dup_hash_idxes.begin(), dup_hash_idxes.end());
  header["hash_index"]["bucket_size"] = bucket_size;

  std::vector<char> comp_key_fst_dst;
  {
    comp_res =
        compress_to_buffer(oss_key_fst_out.str(), oss_key_fst_out.str().size(),
                           comp_key_fst_dst, compress_level);
    if (!comp_res) { return 4; }
    header["key_fst"]["compress_level"] = compress_level;
    header["key_fst"]["original_size"] = oss_key_fst_out.str().size();
    header["key_fst"]["compressed_size"] = comp_key_fst_dst.size();
    fout.write(comp_key_fst_dst.data(), comp_key_fst_dst.size());
    header["key_fst"]["offset"] =
        static_cast<size_t>(header["hash_buckets"]["offset"]) +
        hash_data.size();
  }

  header["meta"]["Creationdate"] = get_current_date();
  std::vector<char> comp_header_dst;
  std::string header_str = header.dump();
  {
    comp_res = compress_to_buffer(header_str.c_str(), header_str.size(),
                                  comp_header_dst, compress_level);
    if (!comp_res) { return 4; }
  }
  fout.write(comp_header_dst.data(), comp_header_dst.size());

  HeaderSizeRecord header_size_record(header_str.size(),
                                      comp_header_dst.size());
  LOG_INFO("{}", header_str);
  LOG_INFO("{},{}", header_size_record.original_size,
           header_size_record.compressed_size);
  fout.write(reinterpret_cast<const char *>(&header_size_record),
             sizeof(HeaderSizeRecord));
  return 0;
}

bool FstdxWriter::extract_fstdx(const std::string &input_file,
                                const std::string &output_file) {
  bool is_valid = false;
  FstdxReader reader(input_file, is_valid);
  if (!is_valid) { return false; }
  ofstream fout(output_file, ios_base::out);
  if (!fout) {
    LOG_ERROR("Failed to open file {} for writing.", output_file);
    return 1;
  }
  const std::vector<std::string> keys = reader.extract_keys();
  const std::vector<std::string> values = reader.extract_values();
  for (size_t i = 0; i < keys.size(); ++i) {
    fout << keys[i] << "\r\n" << values[i] << "\r\n" << DELIMITER << "\r\n";
  }
  return true;
}

std::string FstdxWriter::trim_whitespace(const std::string &s) {
  size_t start = s.find_first_not_of(" \t\n\r");
  if (start == std::string::npos) return "";

  size_t end = s.find_last_not_of(" \t\n\r");
  return s.substr(start, end - start + 1);
}

bool FstdxWriter::parse_raw_txt(std::vector<std::string> &raw_lines,
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

bool FstdxWriter::load_file(const std::string &file_path,
                            std::vector<std::string> &keys,
                            std::vector<std::string> &values) {
  ifstream fin(file_path, ios_base::in);
  if (!fin) {
    LOG_ERROR("Failed to open file {} for reading.", file_path);
    return false;
  }
  return load_file(fin, keys, values);
}

bool FstdxWriter::load_file(ifstream &fin, std::vector<std::string> &keys,
                            std::vector<std::string> &values) {
  LOG_INFO("Loading and parsing raw text file...");
  std::string line;
  vector<string> raw_lines;
  raw_lines.reserve(100000);
  vector<size_t> delimiter_indices;
  size_t index_count = 0;
  const string delimiter(DELIMITER);
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

void FstdxWriter::sort_keys_values(std::vector<std::string> &keys,
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

uint64_t FstdxWriter::get_output(uint32_t index, uint32_t duplicate_count) {
  uint64_t duplicate_mask = static_cast<uint64_t>(duplicate_count) << 32;

  return duplicate_mask | static_cast<uint64_t>(index);
}

void FstdxWriter::make_output(
    std::vector<std::string> &sorted_keys,
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

bool FstdxWriter::compile_fst(
    std::vector<std::pair<std::string, uint64_t>> &input,
    std::ostringstream &oss_out, bool opt_sorted, bool opt_verbose,
    std::function<void(size_t)> progress) {
  LOG_INFO("Compiling FST for {} keys...", input.size());
  return fstd::compile_fst(input, oss_out, opt_sorted, opt_verbose, progress);
}

const nlohmann::json FstdxWriter::meta_default =
    nlohmann::json::array({{{"Version", FSTD_VERSION}},
                           {{"Record", 0}},
                           {{"Format", "Html"}},
                           {{"Keycasesensitive", false}},
                           {{"Stripkey", true}},
                           {{"Description", ""}},
                           {{"Title", ""}},
                           {{"Encoding", "UTF-8"}},
                           {{"Creationdate", ""}},
                           {{"Compressionlevel", 5}},
                           {{"Left2Right", true}},
                           {{"Stylesheet", ""}}});

} // namespace fstd