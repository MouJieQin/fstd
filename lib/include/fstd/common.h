#pragma once
#include <chrono>
#include <fstream>

#include <fstd/logger.h>
#include <indicators/block_progress_bar.hpp>
#include <indicators/cursor_control.hpp>
#include <indicators/dynamic_progress.hpp>
#include <nlohmann/json.hpp>

namespace fstd {

#define FSTD_VERSION "0.1.0"
constexpr auto DELIMITER = "</>";
constexpr size_t max_queue_size = 8;
constexpr size_t disk_read_size =
    512 * 1024; // disk read size: 512kb（default）

using DxJsonHeader = nlohmann::json;
using DdJsonHeader = nlohmann::json;

struct HeaderSizeRecord {
  HeaderSizeRecord() = default;
  HeaderSizeRecord(uint32_t original_size, uint32_t compressed_size);
  uint32_t original_size;
  uint32_t compressed_size;
};

struct BlockIndex {
  BlockIndex() = default;
  BlockIndex(uint32_t end_entry_index, uint64_t block_offset,
             uint32_t block_size, uint32_t original_block_size);
  uint32_t end_entry_index;
  uint64_t block_offset;
  uint32_t block_size;
  uint32_t original_block_size;
};

struct EntryIndex {
  EntryIndex() = default;
  EntryIndex(uint32_t entry_offset, uint32_t entry_size);
  uint32_t entry_offset;
  uint32_t entry_size;
};

template <typename Bar> class DyProgBars {
public:
  DyProgBars() {
    show_progress = is_terminal();
    if (show_progress) { indicators::show_console_cursor(false); }
    bars.set_option(indicators::option::HideBarWhenComplete{false});
  }
  ~DyProgBars() {
    if (show_progress) { indicators::show_console_cursor(true); }
  }

  std::function<void(const size_t)>
  push_back(const size_t total_size, std::string_view prefix_text,
            indicators::Color color = indicators::Color::white) {
    using namespace indicators;
    std::lock_guard<std::mutex> lock(mtx);
    max_prefix_len = std::max(max_prefix_len, prefix_text.size() + 1);
    total_sizes.push_back(total_size);
    last_progresses.push_back(0);
    prefix_texts.push_back(std::string(prefix_text));
    bar_instances.push_back(std::make_unique<Bar>(
        option::BarWidth{80}, option::Start{"|"}, option::End{"|"},
        option::PrefixText{prefix_text}, option::ShowElapsedTime{true},
        option::ShowRemainingTime{true}, option::ForegroundColor{color},
        option::ShowPercentage{true},
        option::FontStyles{std::vector<FontStyle>{FontStyle::bold}}));
    size_t bar_idx = bars.push_back(*bar_instances.back());
    renew_prefix_text();
    return [this, bar_idx](const size_t index) { refresh_bar(bar_idx, index); };
  }

  std::function<void(const size_t, const std::string &)>
  push_back(std::string_view prefix_text,
            indicators::Color color = indicators::Color::white) {
    using namespace indicators;
    std::lock_guard<std::mutex> lock(mtx);
    max_prefix_len = std::max(max_prefix_len, prefix_text.size() + 1);
    total_sizes.push_back(0);
    last_progresses.push_back(0);
    prefix_texts.push_back(std::string(prefix_text));
    bar_instances.push_back(std::make_unique<Bar>(
        option::BarWidth{80}, option::Start{"|"}, option::End{"|"},
        option::PrefixText{prefix_text}, option::ShowElapsedTime{true},
        option::ShowRemainingTime{true}, option::ForegroundColor{color},
        option::ShowPercentage{true},
        option::FontStyles{std::vector<FontStyle>{FontStyle::bold}}));
    size_t bar_idx = bars.push_back(*bar_instances.back());
    renew_prefix_text();
    return
        [this, bar_idx](const size_t count, const std::string &postfix_text) {
          refresh_bar(bar_idx, count, postfix_text);
        };
  }

private:
  void refresh_bar(size_t bar_idx, size_t index) {
    if (show_progress) {
      size_t count = index + 1;
      size_t last_progress = last_progresses[bar_idx];
      size_t progress = count * 100 / total_sizes[bar_idx];
      if (progress > last_progress) {
        bars[bar_idx].set_option(indicators::option::PostfixText{
            std::to_string(count) + "/" +
            std::to_string(total_sizes[bar_idx])});
        bars[bar_idx].set_progress(progress);
        last_progresses[bar_idx] = progress;
      }
      if (count == total_sizes[bar_idx]) { bars[bar_idx].mark_as_completed(); }
    }
  }

  void refresh_bar(size_t bar_idx, size_t count,
                   const std::string &postfix_text) {
    using namespace std::chrono;
    if (show_progress) {
      if (count == 0) {
        bars[bar_idx].set_progress(100);
        bars[bar_idx].mark_as_completed();
      } else {
        bars[bar_idx].set_option(indicators::option::PostfixText{
            std::to_string(count) + " " + postfix_text});
        auto now = std::chrono::steady_clock::now();
        // auto diff_ms = duration_cast<milliseconds>(now - last_update).count();
        auto diff_sec = duration_cast<seconds>(now - last_update).count();
        if (diff_sec >= 1 && last_progresses[bar_idx] < 99) {
          last_progresses[bar_idx] += 1;
          bars[bar_idx].tick();
          last_update = now;
        }
      }
    }
  }

  void renew_prefix_text() {
    for (size_t i = 0; i < bar_instances.size(); i++) {
      if (prefix_texts[i].size() < max_prefix_len) {
        prefix_texts[i] +=
            std::string(max_prefix_len - prefix_texts[i].size(), ' ');
        bars[i].set_option(indicators::option::PrefixText{prefix_texts[i]});
      }
    }
  }

private:
  bool show_progress = true;
  size_t max_prefix_len = 0;
  std::mutex mtx;
  std::vector<std::unique_ptr<Bar>> bar_instances;
  indicators::DynamicProgress<Bar> bars;
  std::vector<std::string> prefix_texts;
  std::vector<size_t> last_progresses;
  std::vector<size_t> total_sizes;
  static std::chrono::steady_clock::time_point last_update;
};
template <typename Bar>
std::chrono::steady_clock::time_point DyProgBars<Bar>::last_update =
    std::chrono::steady_clock::now();

using DyBlockProgBars = DyProgBars<indicators::BlockProgressBar>;

struct CompressTask {
  std::vector<char> src_data;
  size_t index;
};

struct CompressResult {
  std::vector<char> dst_data;
  size_t index;
};

std::string get_current_date();

bool ends_with(std::string const &value, std::string const &ending);

std::string change_ext(const std::string &file_path, const std::string &ext);

bool copy_file(std::istream &ins, const size_t offset, size_t size,
               std::ostream &out);

bool handle_meta(const nlohmann::json &meta, const nlohmann::json &meta_default,
                 nlohmann::json &header);

bool compress_to_buffer(const std::string &src, size_t src_size,
                        std::vector<char> &dst, int compression_level);

bool compress_to_buffer(const char *src, size_t src_size,
                        std::vector<char> &dst, int compression_level);

bool decompress_to_buffer(const void *src, size_t compressed_size,
                          size_t original_size, std::vector<char> &dst);

bool parse_header(std::ifstream &ins, const size_t file_size,
                  nlohmann::json &header);

template <typename T>
bool decompress(std::istream &ins, const std::string &block_name,
                const nlohmann::json &json_header_, std::vector<T> &con) {
  const nlohmann::json &json_block = json_header_[block_name];
  int compress_level = json_block["compress_level"];
  uint64_t offset = json_block["offset"];
  uint64_t original_size = json_block["original_size"];
  std::vector<T> tmp_con;
  tmp_con.resize(original_size / sizeof(T));
  if (compress_level == 0) {
    ins.seekg(offset);
    ins.read(reinterpret_cast<char *>(tmp_con.data()),
             tmp_con.size() * sizeof(T));
  } else {
    uint64_t compressed_size = json_block["compressed_size"];
    std::vector<char> compressed_block(compressed_size);
    ins.seekg(offset);
    ins.read(compressed_block.data(), compressed_block.size());
    std::vector<char> dst_buff(original_size);
    bool res =
        decompress_to_buffer(compressed_block.data(), compressed_block.size(),
                             original_size, dst_buff);
    if (!res) {
      LOG_ERROR("Decompression failed for block: {}", block_name);
      return false;
    }
    memcpy(tmp_con.data(), dst_buff.data(), dst_buff.size());
  }
  con.swap(tmp_con);
  return true;
}

template <typename T>
bool decompress(const std::string &file_path, const std::string &block_name,
                const nlohmann::json &json_header_, std::vector<T> &con) {
  std::ifstream in(file_path, std::ios::binary);
  if (!in) {
    LOG_ERROR("Cannot open the file: {}", file_path);
    return false;
  }
  return decompress(in, block_name, json_header_, con);
}

// ------------------------------
// Sort container by element value
// ------------------------------
template <typename Cont_p>
inline std::vector<size_t> sort_indexes(const Cont_p &input) {
  std::vector<size_t> indices(input.size());
  std::iota(indices.begin(), indices.end(), 0);
  std::sort(indices.begin(), indices.end(),
            [&](size_t i, size_t j) { return input[i] < input[j]; });
  return indices;
}

// ------------------------------
// Specialization: sort vector<pair<...>> by first element
// ------------------------------
template <typename T1, typename T2>
inline std::vector<size_t>
sort_indexes(const std::vector<std::pair<T1, T2>> &input) {
  std::vector<size_t> indices(input.size());
  std::iota(indices.begin(), indices.end(), 0);
  std::sort(indices.begin(), indices.end(), [&](size_t i, size_t j) {
    return input[i].first < input[j].first;
  });
  return indices;
}

// ------------------------------
// Sort container by first pair element
// ------------------------------
template <typename T1, typename T2>
inline std::vector<std::string>
sort_container(std::vector<std::pair<T1, T2>> &&input) {
  auto indices = sort_indexes(input);
  std::vector<std::string> res;
  res.reserve(indices.size());

  for (size_t i : indices) {
    res.emplace_back(std::move(input[i].first));
  }
  return res;
}

inline std::vector<std::string>
sort_container(std::vector<std::string> &&input) {
  auto indices = sort_indexes(input);
  std::vector<std::string> res;
  res.reserve(indices.size());

  for (size_t i : indices) {
    res.emplace_back(std::move(input[i]));
  }
  return res;
}
} // namespace fstd