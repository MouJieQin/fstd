#include <fstream>

#include <fstd/common.h>
#include <fstd/fstdx_compressor.h>
#include <fstd/fstdx_reader.h>
#include <fstd/fstdx_writer.h>
#include <fstd/logger.h>
#include <fstd/thread_pool.h>

using namespace fst;
using namespace std;
using namespace indicators;
using json = nlohmann::json;

namespace fstd {

int FstdxWriter::compile_fstdx(const std::string &output_file,
                               std::vector<std::string> &&keys,
                               std::vector<std::string> &&values,
                               const json &meta, uint16_t block_size_kb,
                               uint8_t compress_level,
                               uint16_t zstd_dict_size_kb, size_t worker_num,
                               bool opt_sorted, bool opt_verbose) const {
  std::filesystem::path path_obj(
      reinterpret_cast<const char8_t *>(output_file.c_str()));
  ofstream fout(path_obj, ios_base::binary);
  if (!fout) {
    LOG_ERROR("Failed to open file {} for writing.", path_obj.string());
    return 1;
  }
  return compile_fstdx(fout, std::move(keys), std::move(values), meta,
                       block_size_kb, compress_level, zstd_dict_size_kb,
                       worker_num, opt_sorted, opt_verbose);
}

int FstdxWriter::compile_fstdx(const std::string &output_file,
                               std::vector<std::string> &&keys,
                               std::vector<std::string> &&values,
                               const std::string &meta_json_str,
                               uint16_t block_size_kb, uint8_t compress_level,
                               uint16_t zstd_dict_size_kb, size_t worker_num,
                               bool opt_sorted, bool opt_verbose) const {

  json meta_json;
  try {
    meta_json = json::parse(meta_json_str);
  } catch (const json::exception &e) {
    LOG_ERROR("JSON string {} format error: {}", meta_json_str, e.what());
    return 1;
  } catch (const std::exception &e) {
    LOG_ERROR("JSON string {} read error: {}", meta_json_str, e.what());
    return 1;
  }
  return compile_fstdx(output_file, std::move(keys), std::move(values),
                       meta_json, block_size_kb, compress_level,
                       zstd_dict_size_kb, worker_num, opt_sorted, opt_verbose);
}

int FstdxWriter::compile_fstdx(const std::string &input_file,
                               const std::string &output_file,
                               const std::string &meta_json_str,
                               uint16_t block_size_kb, uint8_t compress_level,
                               uint16_t zstd_dict_size_kb, size_t worker_num,
                               bool opt_sorted, bool opt_verbose) const {
  json meta_json;
  try {
    meta_json = json::parse(meta_json_str);
  } catch (const json::exception &e) {
    LOG_ERROR("JSON string {} format error: {}", meta_json_str, e.what());
    return 1;
  } catch (const std::exception &e) {
    LOG_ERROR("JSON string {} read error: {}", meta_json_str, e.what());
    return 1;
  }
  return compile_fstdx(input_file, output_file, meta_json, block_size_kb,
                       compress_level, zstd_dict_size_kb, worker_num,
                       opt_sorted, opt_verbose);
}

int FstdxWriter::compile_fstdx(const std::string &input_file,
                               const std::string &output_file, const json &meta,
                               uint16_t block_size_kb, uint8_t compress_level,
                               uint16_t zstd_dict_size_kb, size_t worker_num,
                               bool opt_sorted, bool opt_verbose) const {
  std::filesystem::path out_path_obj(
      reinterpret_cast<const char8_t *>(output_file.c_str()));
  ofstream fout(out_path_obj, ios_base::binary);
  if (!fout) {
    LOG_ERROR("Failed to open file {} for writing.", out_path_obj.string());
    return 1;
  }
  std::filesystem::path in_path_obj(
      reinterpret_cast<const char8_t *>(input_file.c_str()));
  ifstream fin(in_path_obj, ios_base::binary);
  if (!fin) {
    LOG_ERROR("Failed to open file {} for reading.", in_path_obj.string());
    return 1;
  }

  if (!ends_with(input_file, ".fstdx")) {
    // Load file as text
    vector<string> keys;
    vector<string> values;
    LOG_INFO("Loading file {}...", input_file);
    if (!load_file(fin, keys, values)) { return 1; }
    LOG_INFO("Loaded {} keys and values.", keys.size());
    fin.close();
    return compile_fstdx(fout, std::move(keys), std::move(values), meta,
                         block_size_kb, compress_level, zstd_dict_size_kb,
                         worker_num, opt_sorted, opt_verbose);
  } else {
    // Load fstdx file
    FstdxReader reader(input_file);
    if (!reader) {
      LOG_ERROR("Invalid fstdx file {}.", input_file);
      return 2;
    }
    DxJsonHeader header(reader.get_header());
    nlohmann::json meta_default = nlohmann::json::array();
    for (auto &[key, val] : reader.get_header()["meta"].items()) {
      nlohmann::json single_obj;
      single_obj[key] = val;
      meta_default.push_back(single_obj);
    }
    if (!handle_meta(meta, meta_default, header)) { return 5; }
    if (compress_level == header["meta"]["Compressionlevel"].get<size_t>() &&
        zstd_dict_size_kb * 1024 ==
            header["comp_dict"]["original_size"].get<size_t>() &&
        block_size_kb * 1024 ==
            header["comp_blocks"]["block_size"].get<size_t>()) {
      size_t copy_size = header["key_fst"]["offset"].get<size_t>() +
                         header["key_fst"]["compressed_size"].get<size_t>();
      if (!copy_file(fin, 0, copy_size, fout)) { return 6; }
    } else {
      vector<string> values = reader.extract_values();
      FstdxCompressor compressor;
      LOG_INFO("Compressing values...");
      if (worker_num == 0) { worker_num = get_cpu_core_count(); }
      ThreadPool thread_pool(worker_num);
      DyBlockProgBars dynamic_bars;
      if (!compressor.compress_texts_to_stream(
              fout, values, header, zstd_dict_size_kb * 1024,
              block_size_kb * 1024, compress_level, thread_pool,
              dynamic_bars)) {
        return 3;
      }
    }
    std::vector<char> key_fst_byte_code;
    if (!decompress(fin, "key_fst", reader.get_header(), key_fst_byte_code)) {
      return 7;
    }
    header["comp_dict"]["original_size"] = zstd_dict_size_kb * 1024;
    header["comp_blocks"]["block_size"] = block_size_kb * 1024;
    header["meta"]["Compressionlevel"] = compress_level;
    ostringstream oss_key_fst_out(ios_base::binary);
    oss_key_fst_out.write(key_fst_byte_code.data(), key_fst_byte_code.size());
    return write_fst_header(fout, oss_key_fst_out, header, compress_level);
  }
}

int FstdxWriter::compile_fstdx(std::ostream &fout,
                               std::vector<std::string> &&keys,
                               std::vector<std::string> &&values,
                               const json &meta, uint16_t block_size_kb,
                               uint8_t compress_level,
                               uint16_t zstd_dict_size_kb, size_t worker_num,
                               bool opt_sorted, bool opt_verbose) const {
  DxJsonHeader header;
  if (!handle_meta(meta, meta_default, header)) { return 5; }
  header["comp_dict"]["original_size"] = zstd_dict_size_kb * 1024;
  header["comp_blocks"]["block_size"] = block_size_kb * 1024;
  header["meta"]["Record"] = keys.size();
  header["meta"]["Stripkey"] = true;
  header["meta"]["Compressionlevel"] = compress_level;
  if (!opt_sorted) { sort_keys_values(keys, values); }
  vector<pair<string, uint64_t>> input;
  make_output(std::move(keys), input);
  header["key_fst"]["keys_size"] = input.size();
  {
    // release keys memory
    vector<string> tmp;
    keys.swap(tmp);
  }
  return compile_fstdx_impl(fout, input, std::move(values), header,
                            block_size_kb, compress_level, zstd_dict_size_kb,
                            worker_num, opt_verbose);
}

int FstdxWriter::compile_fstdx_impl(
    std::ostream &fout, std::vector<std::pair<std::string, uint64_t>> &input,
    std::vector<std::string> &&values, DxJsonHeader &header,
    uint16_t block_size_kb, uint8_t compress_level, uint16_t zstd_dict_size_kb,
    size_t worker_num, bool opt_verbose) const {

  if (worker_num == 0) { worker_num = get_cpu_core_count(); }
  ThreadPool thread_pool(worker_num);
  DyBlockProgBars dynamic_bars;

  ostringstream oss_key_fst_out(ios_base::binary);
  auto compile_res = thread_pool.enqueue([&]() {
    auto refresh_bar =
        dynamic_bars.push_back(input.size(), "Compiling key FST:", Color::cyan);
    bool res =
        compile_fst(input, oss_key_fst_out, true, opt_verbose, refresh_bar);
    if (res) {
      LOG_DEBUG("FST compiled.");
    } else {
      LOG_ERROR("Compile FST failed.");
    }
    return res;
  });

  FstdxCompressor compressor;
  LOG_INFO("Compressing values...");
  if (!compressor.compress_texts_to_stream(
          fout, std::move(values), header, zstd_dict_size_kb * 1024,
          block_size_kb * 1024, compress_level, thread_pool, dynamic_bars)) {
    return 3;
  }
  if (!compile_res.get()) { return 1; }

  return write_fst_header(fout, oss_key_fst_out, header, compress_level);
}

int FstdxWriter::write_fst_header(std::ostream &fout,
                                  std::ostringstream &oss_key_fst_out,
                                  DxJsonHeader &header,
                                  uint8_t compress_level) const {
  bool comp_res = false;
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
        header["entry_indexes"]["offset"].get<size_t>() +
        header["meta"]["Record"].get<size_t>() * sizeof(EntryIndex);
  }

  {
    header["meta"]["Creationdate"] = get_current_date();
    std::vector<char> comp_header_dst;
    std::string header_str = header.dump();
    comp_res = compress_to_buffer(header_str.c_str(), header_str.size() + 1,
                                  comp_header_dst, compress_level);
    if (!comp_res) { return 4; }
    fout.write(comp_header_dst.data(), comp_header_dst.size());

    HeaderSizeRecord header_size_record(header_str.size() + 1,
                                        comp_header_dst.size());
    LOG_DEBUG("{},{}", header_size_record.original_size,
              header_size_record.compressed_size);
    LOG_INFO("{}", header.dump(2));
    fout.write(reinterpret_cast<const char *>(&header_size_record),
               sizeof(HeaderSizeRecord));
  }
  return 0;
}

std::string FstdxWriter::trim_whitespace(const std::string &s) const {
  size_t start = s.find_first_not_of(" \t\n\r");
  if (start == std::string::npos) return "";

  size_t end = s.find_last_not_of(" \t\n\r");
  return s.substr(start, end - start + 1);
}

bool FstdxWriter::parse_raw_txt(std::vector<unique_ptr<string>> &raw_lines,
                                std::vector<size_t> &delimiter_indices,
                                std::vector<std::string> &keys,
                                std::vector<std::string> &values) const {
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
  keys_temp.emplace_back(trim_whitespace(*raw_lines[0]));
  for (size_t i = 0; i < delimiter_indices.size() - 1; i++) {
    keys_temp.emplace_back(
        trim_whitespace(*raw_lines[delimiter_indices[i] + 1]));
  }

  vector<string> values_temp;
  values_temp.reserve(delimiter_indices.size());
  string cur_value("");
  // handle the first value
  for (size_t i = 1; i < delimiter_indices.front(); i++) {
    cur_value += *raw_lines[i] + "\n";
  }
  // remove the last '\n'
  cur_value.pop_back();
  values_temp.emplace_back(std::move(cur_value));
  for (size_t i = 0; i < delimiter_indices.size() - 1; i++) {
    cur_value = "";
    for (size_t j = delimiter_indices[i] + 2; j < delimiter_indices[i + 1];
         j++) {
      cur_value += *raw_lines[j] + "\n";
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
                            std::vector<std::string> &values) const {
  std::filesystem::path path_obj(
      reinterpret_cast<const char8_t *>(file_path.c_str()));
  ifstream fin(path_obj, ios_base::in);
  if (!fin) {
    LOG_ERROR("Failed to open file {} for reading.", path_obj.string());
    return false;
  }
  return load_file(fin, keys, values);
}

bool FstdxWriter::load_file(ifstream &fin, std::vector<std::string> &keys,
                            std::vector<std::string> &values) const {
  LOG_INFO("Loading and parsing raw text file...");
  string line;
  vector<unique_ptr<string>> raw_lines;
  raw_lines.reserve(100000);
  vector<size_t> delimiter_indices;
  size_t index_count = 0;
  const string delimiter(DELIMITER);
  while (getline(fin, line)) {
    string trimmed_line = trim_whitespace(line);
    if (trimmed_line == delimiter) {
      delimiter_indices.emplace_back(index_count);
    }
    if (!line.empty() && line.back() == '\r') { line.pop_back(); }
    raw_lines.emplace_back(make_unique<string>(std::move(line)));
    index_count += 1;
  }
  return parse_raw_txt(raw_lines, delimiter_indices, keys, values);
}

void FstdxWriter::sort_keys_values(std::vector<std::string> &keys,
                                   std::vector<std::string> &values) const {
  LOG_INFO("Sorting {} keys and values...", keys.size());
  vector<size_t> sorted_indices = sort_indexes(keys);
  vector<string> temp_keys;
  temp_keys.reserve(keys.size());
  vector<string> temp_values;
  temp_values.reserve(values.size());
  for (size_t i : sorted_indices) {
    temp_keys.emplace_back(std::move(keys[i]));
    temp_values.emplace_back(std::move(values[i]));
  }
  keys.swap(temp_keys);
  values.swap(temp_values);
}

uint64_t FstdxWriter::get_output(uint32_t index,
                                 uint32_t duplicate_count) const {
  uint64_t duplicate_mask = static_cast<uint64_t>(duplicate_count) << 32;
  return duplicate_mask | static_cast<uint64_t>(index);
}

void FstdxWriter::make_output(
    std::vector<std::string> &&sorted_keys,
    std::vector<std::pair<std::string, uint64_t>> &input) const {
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
  // handle the last key
  if (duplicate_count > 0) { total_duplicate_count += 1; }
  LOG_INFO("Total duplicate keys: {}", total_duplicate_count);
  uint64_t output = get_output(index, duplicate_count);
  temp_input.emplace_back(std::move(sorted_keys[index]), output);
  input.swap(temp_input);
}

bool FstdxWriter::compile_fst(
    std::vector<std::pair<std::string, uint64_t>> &input,
    std::ostringstream &oss_out, bool opt_sorted, bool opt_verbose,
    std::function<void(size_t)> progress) const {
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