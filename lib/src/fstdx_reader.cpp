#include <fstd/fstdx_reader.h>
#include <fstd/logger.h>
namespace fstd {

using namespace std;
using namespace indicators;
using json = nlohmann::json;

FstdxHashReader::FstdxHashReader(const std::string &fstdx_path)
    : fstdx_path_(fstdx_path), key_size_(0), ddict_(nullptr) {
  if (!parse_fstdx(fstdx_path_)) {
    is_valid_ = false;
    return;
  }
  is_valid_ = true;
}

FstdxHashReader::~FstdxHashReader() { ZSTD_freeDDict(ddict_); }

FstdxHashReader::operator bool() const { return is_valid_; }

size_t FstdxHashReader::get_key_size() const { return key_size_; }

const json &FstdxHashReader::get_meta() const {
  return mx_json_header_["meta"];
}

const DxJsonHeader &FstdxHashReader::get_header() const {
  return mx_json_header_;
}

bool FstdxHashReader::parse_fstdx(const std::string &fstdx_path) {
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

  std::vector<char> dictBuffer;
  if (!decompress(ins, "comp_dict", mx_json_header_, dictBuffer)) {
    return false;
  }
  ddict_ = ZSTD_createDDict(dictBuffer.data(), dictBuffer.size());

  if (!decompress(ins, "block_indexes", mx_json_header_, block_indexes_)) {
    return false;
  }
  ins.close();

  key_size_ = mx_json_header_["meta"]["Record"];
  comp_text_offset_ = mx_json_header_["comp_blocks"]["offset"];
  entry_indexes_offset_ = mx_json_header_["entry_indexes"]["offset"];
  return true;
}

size_t FstdxHashReader::bin_search_block_index(
    uint32_t entry_index, const std::vector<BlockIndex> &block_indexes) const {
  size_t first = 0;
  size_t last = block_indexes.size();
  size_t len = last - first;
  while (len > 1) {
    size_t half = len >> 1;
    size_t middle = first + half - 1;
    uint32_t middle_end_index = block_indexes[middle].end_entry_index;
    if (entry_index == middle_end_index) {
      return middle + 1;
    } else if (entry_index < middle_end_index) {
      last = middle + 1;
    } else {
      first = middle + 1;
    }
    len = last - first;
  }
  return first;
}

bool FstdxHashReader::read_entry_index(std::ifstream &in, const size_t idx,
                                       EntryIndex &entry_index) const {

  in.seekg(entry_indexes_offset_ + idx * sizeof(EntryIndex));
  in.read(reinterpret_cast<char *>(&entry_index), sizeof(EntryIndex));
  return true;
}

std::pair<uint32_t, uint32_t>
FstdxHashReader::extract_index(uint64_t index) const {
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

bool FstdxHashReader::exact_match_search_by_index_code(
    uint64_t idx_code, std::vector<std::string> &result) const {
  auto [index, duplicate] = extract_index(idx_code);
  LOG_DEBUG("index: {}, duplicate: {}", index, duplicate);
  std::vector<std::string> tmp_result;
  for (uint32_t i = 0; i <= duplicate; ++i) {
    std::string text = read_text_by_index(index + i);
    tmp_result.emplace_back(std::move(text));
  }
  result.swap(tmp_result);
  return true;
}

// bool FstdxHashReader::hash_exact_match_search(
//     std::string_view word, std::vector<std::string> &result) const {
//   uint64_t index_res = 0;
//   string key(word);
//   if (!read_hash_index(fstdx_path_, key, index_res, dup_idxes_, bucket_size_,
//                        hash_bucket_offset_, hash_index_offset_)) {
//     return false;
//   }
//   return exact_match_search_by_index_code(index_res, result);
// }

std::string FstdxHashReader::read_text_by_index(const size_t idx) const {
  std::ifstream comp_in(fstdx_path_, std::ios::binary);
  if (!comp_in) {
    LOG_ERROR("Couldn't open the file: {}", fstdx_path_);
    return "";
  }
  EntryIndex entry_index;
  if (!read_entry_index(comp_in, idx, entry_index)) { return ""; }

  size_t block_index_idx = bin_search_block_index(idx, block_indexes_);
  const BlockIndex &block_index = block_indexes_[block_index_idx];

  LOG_DEBUG("block_index.end_entry_index: {}, block_index.block_offset: {}, "
            "block_index.block_size: {}, block_index.original_block_size: {}",
            block_index.end_entry_index, block_index.block_offset,
            block_index.block_size, block_index.original_block_size);
  LOG_DEBUG("entry_index.entry_offset: {}, entry_index.entry_size: {}",
            entry_index.entry_offset, entry_index.entry_size);

  comp_in.seekg(comp_text_offset_ + block_index.block_offset);
  std::vector<char> comp_buf(block_index.block_size);
  comp_in.read(comp_buf.data(), comp_buf.size());

  unsigned long long decomp_buf_size = block_index.original_block_size;
  std::vector<char> decomp_buf(decomp_buf_size);
  ZSTD_DCtx *dctx = ZSTD_createDCtx();
  if (!dctx) {
    LOG_ERROR("Create Decompression context failed.");
    return "";
  }
  size_t decomp_size =
      ZSTD_decompress_usingDDict(dctx, decomp_buf.data(), decomp_buf.size(),
                                 comp_buf.data(), comp_buf.size(), ddict_);
  ZSTD_freeDCtx(dctx);
  if (ZSTD_isError(decomp_size)) {
    LOG_ERROR("Decompression failed: {}", ZSTD_getErrorName(decomp_size));
    return "";
  }

  return std::string(decomp_buf.data() + entry_index.entry_offset,
                     entry_index.entry_size);
}

std::vector<std::string> FstdxHashReader::extract_comp_blocks(
    std::function<void(size_t)> refresh_bar) const {
  std::vector<std::string> result;
  std::ifstream comp_in(fstdx_path_, std::ios::binary);
  if (!comp_in) {
    LOG_ERROR("Couldn't open file: {}", fstdx_path_);
    return result;
  }

  ZSTD_DCtx *dctx = ZSTD_createDCtx();
  if (!dctx) {
    LOG_ERROR("Create Decompression context failed.");
    return result;
  }

  size_t idx = 0;
  std::vector<char> comp_buf;
  std::vector<char> decomp_buf;
  std::vector<EntryIndex> entry_indexes;
  if (!decompress(comp_in, "entry_indexes", mx_json_header_, entry_indexes)) {
    return result;
  }
  result.reserve(entry_indexes.size());
  for (const BlockIndex &block_index : block_indexes_) {
    comp_in.seekg(comp_text_offset_ + block_index.block_offset);
    comp_buf.resize(block_index.block_size);
    comp_in.read(comp_buf.data(), comp_buf.size());

    decomp_buf.resize(block_index.original_block_size);
    size_t decomp_size =
        ZSTD_decompress_usingDDict(dctx, decomp_buf.data(), decomp_buf.size(),
                                   comp_buf.data(), comp_buf.size(), ddict_);
    if (ZSTD_isError(decomp_size)) {
      ZSTD_freeDCtx(dctx);
      LOG_ERROR("Decompression failed: {}", ZSTD_getErrorName(decomp_size));
      return {};
    }

    for (; idx < block_index.end_entry_index; ++idx) {
      const char *data_ptr = decomp_buf.data();
      const EntryIndex &entry_index = entry_indexes[idx];
      result.emplace_back(std::string(data_ptr + entry_index.entry_offset,
                                      entry_index.entry_size));
      if (refresh_bar) { refresh_bar(idx); }
    }
  }
  ZSTD_freeDCtx(dctx);
  return result;
}

std::vector<std::string> FstdxHashReader::extract_values(
    DyProgBars<indicators::BlockProgressBar> &dynamic_bars) const {
  auto refresh_bar = dynamic_bars.push_back(
      key_size_, "Decompressing value blocks:", Color::white);

  return extract_comp_blocks(refresh_bar);
}

std::vector<std::string> FstdxHashReader::extract_values() const {
  return extract_comp_blocks();
}

FstdxReader::FstdxReader(const std::string &fstdx_path)
    : FstdxHashReader(fstdx_path), fst_key_size_(0) {
  if (!FstdxHashReader::operator bool()) { return; }
  fst_key_size_ = mx_json_header_["key_fst"]["keys_size"];
  std::vector<char> key_fst_byte_code;
  if (!decompress(fstdx_path, "key_fst", mx_json_header_, key_fst_byte_code)) {
    is_valid_ = false;
    return;
  }
  fst_map_searcher_ = FstMapSearcher<uint64_t>(std::move(key_fst_byte_code));
  is_valid_ = true;
}

size_t FstdxReader::get_fst_key_size() const { return fst_key_size_; }

bool FstdxReader::exact_match_search(std::string_view word,
                                     std::vector<std::string> &result) const {
  uint64_t index_res = 0;
  if (!fst_map_searcher_.exact_match_search(word, index_res)) { return false; }
  return exact_match_search_by_index_code(index_res, result);
}

std::vector<std::unique_ptr<std::string>>
FstdxReader::common_prefix_search(std::string_view word) const {
  return fst_map_searcher_.common_prefix_search(word);
}

size_t FstdxReader::longest_prefix_len(std::string_view word) const {
  return fst_map_searcher_.longest_prefix_len(word);
}

std::vector<std::unique_ptr<std::string>>
FstdxReader::predictive_search(std::string_view word) const {
  return fst_map_searcher_.predictive_search(word);
}

std::vector<std::unique_ptr<std::string>>
FstdxReader::edit_distance_search(std::string_view word,
                                  size_t edit_distance) const {
  return fst_map_searcher_.edit_distance_search(word, edit_distance);
}

std::vector<std::vector<std::unique_ptr<std::string>>>
FstdxReader::prefix_distance_search(
    std::string_view sv, size_t max_distance, const size_t longest_prefix_len,
    const std::shared_ptr<std::set<std::string>> &prior_suffixes) const {
  return fst_map_searcher_.prefix_distance_search(
      sv, max_distance, longest_prefix_len, prior_suffixes);
}

std::vector<std::unique_ptr<std::pair<double, std::string>>>
FstdxReader::suggest(std::string_view word) const {
  return fst_map_searcher_.suggest(word);
}

std::pair<std::vector<std::unique_ptr<std::string>>, std::string>
FstdxReader::regex_search(std::string_view pattern) const {
  return fst_map_searcher_.regex_search(pattern);
}

std::pair<std::vector<std::unique_ptr<std::string>>, std::string>
FstdxReader::regex_search(std::string_view pattern,
                          ThreadPool &thread_pool) const {
  return fst_map_searcher_.regex_search(pattern, thread_pool);
}

// std::vector<std::tuple<double, std::string, uint64_t>>
// FstdxReader::spellcheck_word(std::string_view word, const size_t n) const {
//   return fst_map_searcher_.spellcheck_word(word, n);
// }

std::vector<std::pair<std::string, uint64_t>> FstdxReader::enumerate() const {
  return fst_map_searcher_.enumerate(fst_key_size_);
}

std::vector<std::pair<std::string, uint64_t>>
FstdxReader::enumerate(std::function<void(const size_t)> refresh_bar) const {
  return fst_map_searcher_.enumerate(fst_key_size_, refresh_bar);
}

bool FstdxReader::extract(const std::string &output_file) {
  ofstream fout(output_file, ios_base::out);
  if (!fout) {
    LOG_ERROR("Failed to open file {} for writing.", output_file);
    return false;
  }

  DyProgBars<BlockProgressBar> dynamic_bars;
  const std::vector<std::string> keys = extract_keys(dynamic_bars);
  const std::vector<std::string> values = extract_values(dynamic_bars);

  auto refresh_bar =
      dynamic_bars.push_back(key_size_, "Writing:", Color::green);

  for (size_t i = 0; i < keys.size(); ++i) {
    fout << keys[i] << "\n" << values[i] << "\n" << DELIMITER << "\n";
    refresh_bar(i);
  }
  return true;
}

std::vector<std::string> FstdxReader::extract_keys(
    DyProgBars<indicators::BlockProgressBar> &dynamic_bars) const {
  auto refresh_bar = dynamic_bars.push_back(
      fst_key_size_, "Decompiling key FST:", Color::cyan);

  return extract_keys(enumerate(refresh_bar));
}

std::vector<std::string> FstdxReader::extract_keys() const {
  return extract_keys(enumerate());
}

std::vector<std::string> FstdxReader::extract_keys(
    std::vector<std::pair<std::string, uint64_t>> &&key_output) const {
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

} // namespace fstd