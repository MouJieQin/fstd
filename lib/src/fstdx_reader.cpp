
#include <fstd/fstdx_reader.h>
#include <fstd/logger.h>

namespace fstd {

using namespace std;
using json = nlohmann::json;

FstdxReader::FstdxReader(const std::string &fstdx_path, bool &is_valid)
    : fstdx_path_(fstdx_path), key_size_(0), fst_key_size_(0), ddict_(nullptr) {
  if (!parse_fstdx(fstdx_path_)) {
    is_valid = false;
    return;
  }
  is_valid = true;
}

FstdxReader::~FstdxReader() { ZSTD_freeDDict(ddict_); }

const json &FstdxReader::get_meta() const { return mx_json_header_["meta"]; }

size_t FstdxReader::get_key_size() const { return key_size_; }

size_t FstdxReader::get_fst_key_size() const { return fst_key_size_; }

std::pair<uint32_t, uint32_t> FstdxReader::extract_index(uint64_t index) const {
  uint32_t index_ = 0;
  uint32_t duplicate = 0;
  if (index < UINT32_MAX) {
    index_ = static_cast<uint32_t>(index);
  } else {
    index_ = static_cast<uint32_t>(index & 0xFFFFFFFF);
    duplicate = static_cast<uint32_t>(index >> 32);
  }
  return {index_, duplicate};
}

bool FstdxReader::exact_match_search(std::string_view word,
                                     std::vector<std::string> &result) const {
  uint64_t index_res = 0;
  if (!fst_map_searcher_.exact_match_search(word, index_res)) { return false; }
  auto [index, duplicate] = extract_index(index_res);
  LOG_INFO("index: {}, duplicate: {}", index, duplicate);
  std::vector<std::string> tmp_result;
  for (uint32_t i = 0; i <= duplicate; ++i) {
    LOG_INFO("index + i: {}, block_indexes_.size(): {}, entry_indexes_.size(): "
             "{}, compstdx_path_: {}, comp_text_offset_: {}",
             index + i, block_indexes_.size(), entry_indexes_.size(),
             fstdx_path_, comp_text_offset_);
    std::string text = dx_compressor_.readTextByIndex(
        index + i, ddict_, block_indexes_, entry_indexes_, fstdx_path_,
        comp_text_offset_);
    tmp_result.emplace_back(std::move(text));
  }
  result.swap(tmp_result);
  return true;
}

std::vector<std::pair<std::string, uint64_t>>
FstdxReader::common_prefix_search(std::string_view word) const {
  return fst_map_searcher_.common_prefix_search(word);
}

size_t FstdxReader::longest_common_prefix_search(
    std::string_view word, std::pair<std::string, uint64_t> &result) const {
  return fst_map_searcher_.longest_common_prefix_search(word, result);
}

std::vector<std::pair<std::string, uint64_t>>
FstdxReader::predictive_search(std::string_view word) const {
  return fst_map_searcher_.predictive_search(word);
}

std::vector<std::pair<std::string, uint64_t>>
FstdxReader::edit_distance_search(std::string_view word,
                                  size_t edit_distance) const {
  return fst_map_searcher_.edit_distance_search(word, edit_distance);
}

std::pair<std::vector<std::pair<std::string, uint64_t>>, std::string>
FstdxReader::regex_search(std::string_view pattern) const {
  return fst_map_searcher_.regex_search(pattern);
}

std::vector<std::tuple<double, std::string, uint64_t>>
FstdxReader::spellcheck_word(std::string_view word, const size_t n) const {
  return fst_map_searcher_.spellcheck_word(word, n);
}

std::vector<std::pair<std::string, uint64_t>> FstdxReader::enumerate() const {
  return fst_map_searcher_.enumerate(fst_key_size_);
}

std::vector<std::string> FstdxReader::extract_values() const {
  return dx_compressor_.extract(fstdx_path_, comp_text_offset_, ddict_,
                                block_indexes_, entry_indexes_);
}

std::vector<std::string> FstdxReader::extract_keys() const {
  std::vector<std::pair<std::string, uint64_t>> key_output = enumerate();
  std::vector<std::string> result;
  result.resize(key_size_);
  for (size_t i = 0; i < key_output.size(); ++i) {
    auto [index, duplicate] = extract_index(key_output[i].second);
    result[index] = std::move(key_output[i].first);
    for (uint32_t j = 1; j <= duplicate; ++j) {
      result[index + j] = result[index];
    }
  }
  return result;
}

bool FstdxReader::parse_fstdx(const std::string &fstdx_path) {
  std::ifstream ins(fstdx_path, std::ios::binary | std::ios::ate);
  if (!ins) {
    LOG_ERROR("Cannot open the file: {}", fstdx_path);
    return false;
  }
  size_t fstdx_size = ins.tellg();
  if (!parse_header(ins, fstdx_size, mx_json_header_)) { return false; }
  size_t record_size = sizeof(HeaderSizeRecord);
  if (fstdx_size < record_size) {
    LOG_ERROR("It is not a valid fstdx file: {}", fstdx_path);
    return false;
  }

  std::vector<char> key_fst_byte_code;
  if (!decompress(ins, "key_fst", mx_json_header_, key_fst_byte_code)) {
    return false;
  }
  fst_map_searcher_ = FstMapSearcher<uint64_t>(std::move(key_fst_byte_code));

  std::vector<char> dictBuffer;
  if (!decompress(ins, "comp_dict", mx_json_header_, dictBuffer)) {
    return false;
  }
  ddict_ = ZSTD_createDDict(dictBuffer.data(), dictBuffer.size());

  if (!decompress(ins, "block_indexes", mx_json_header_, block_indexes_)) {
    return false;
  }

  if (!decompress(ins, "entry_indexes", mx_json_header_, entry_indexes_)) {
    return false;
  }

  ins.close();

  comp_text_offset_ = mx_json_header_["comp_blocks"]["offset"];
  key_size_ = mx_json_header_["meta"]["Record"];
  fst_key_size_ = mx_json_header_["key_fst"]["keys_size"];
  return true;
}

} // namespace fstd