#include <fstd/common.h>
#include <zstd.h>
namespace fstd {

using namespace std;
using json = nlohmann::json;

HeaderSizeRecord::HeaderSizeRecord(uint32_t original_size,
                                   uint32_t compressed_size)
    : original_size(original_size), compressed_size(compressed_size) {}

BlockIndex::BlockIndex(uint32_t end_entry_index, uint64_t block_offset,
                       uint32_t block_size, uint32_t original_block_size)
    : end_entry_index(end_entry_index), block_offset(block_offset),
      block_size(block_size), original_block_size(original_block_size) {}

EntryIndex::EntryIndex(uint32_t entry_offset, uint32_t entry_size)
    : entry_offset(entry_offset), entry_size(entry_size) {}

std::string get_current_date() {
  std::time_t now = std::time(nullptr);
  std::tm local_tm = *std::localtime(&now);
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", local_tm.tm_year + 1900,
                local_tm.tm_mon + 1, local_tm.tm_mday);
  return std::string(buf);
}

std::string lf_to_crlf(const char *src, size_t n) {
  std::string dst;
  dst.reserve(n + n / 16);
  for (size_t i = 0; i < n; i++) {
    char ch = src[i];
    if (ch == '\n') {
      dst += "\r\n";
    } else {
      dst.push_back(ch);
    }
  }
  return dst;
}

bool ends_with(std::string const &value, std::string const &ending) {
  if (ending.size() > value.size()) return false;
  return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

bool read_file(const std::string &file_path, std::string &content) {
  std::ifstream file_stream{std::filesystem::path(file_path)};
  if (!file_stream) { return false; }
  content = std::string((std::istreambuf_iterator<char>(file_stream)),
                        std::istreambuf_iterator<char>());
  return true;
}

bool copy_file(std::istream &ins, const size_t offset, size_t size,
               std::ostream &out) {
  ins.seekg(offset, ios::beg);
  char buff[disk_read_size];
  while (ins && size > 0) {
    LOG_INFO("size: {}", size);
    if (size >= disk_read_size) {
      ins.read(buff, disk_read_size);
    } else {
      ins.read(buff, size);
    }
    size_t read_size = ins.gcount();
    out.write(buff, read_size);
    size -= read_size;
  }
  if (size > 0) {
    LOG_ERROR("Copy file to file failed");
    return false;
  }
  return true;
}

bool handle_meta(const nlohmann::json &meta, const nlohmann::json &meta_default,
                 nlohmann::json &header) {
  if (!meta.is_object()) {
    LOG_ERROR("Meta is not an object.");
    return false;
  }

  json &h_meta = header["meta"];
  // write meta_default to header["meta"]
  for (const auto &item : meta_default) {

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

bool compress_to_buffer(const std::string &src, size_t src_size,
                        std::vector<char> &dst, int compression_level) {
  return compress_to_buffer(src.c_str(), src_size, dst, compression_level);
}

bool compress_to_buffer(const char *src, size_t src_size,
                        std::vector<char> &dst, int compression_level) {
  size_t comp_buf_size = ZSTD_compressBound(src_size);
  std::vector<char> comp_buf(comp_buf_size);
  size_t comp_size = ZSTD_compress(comp_buf.data(), comp_buf.size(), src,
                                   src_size, compression_level);
  if (ZSTD_isError(comp_size)) {
    LOG_ERROR("Compression failed: {}", ZSTD_getErrorName(comp_size));
    return false;
  }
  comp_buf.resize(comp_size);
  dst.swap(comp_buf);
  return true;
}

bool decompress_to_buffer(const void *src, size_t compressed_size,
                          size_t original_size, std::vector<char> &dst) {
  std::vector<char> decomp_buf(original_size);
  size_t actual_decomp_size =
      ZSTD_decompress(decomp_buf.data(), original_size, src, compressed_size);
  if (ZSTD_isError(actual_decomp_size)) {
    LOG_ERROR("Decompression failed: {}",
              ZSTD_getErrorName(actual_decomp_size));
    return false;
  }
  dst.swap(decomp_buf);
  return true;
}

bool parse_header(std::ifstream &ins, const size_t file_size,
                  nlohmann::json &header) {
  size_t record_size = sizeof(HeaderSizeRecord);
  if (file_size < record_size) {
    LOG_ERROR("It is not a valid fstdx/fstdd file.");
    return false;
  }
  LOG_INFO("file_size:{}", file_size);
  HeaderSizeRecord header_size_record;
  LOG_INFO("record_size:{}", record_size);
  ins.seekg(-record_size, std::ios::end);
  ins.read(reinterpret_cast<char *>(&header_size_record), record_size);
  LOG_INFO("header_size_record: original_size:{}, compressed_size:{}",
           header_size_record.original_size,
           header_size_record.compressed_size);
  if (file_size < header_size_record.compressed_size ||
      (header_size_record.compressed_size == 0 ||
       header_size_record.original_size == 0) ||
      (header_size_record.original_size > 1024 * 1024 * 5)) {
    LOG_ERROR("It is not a valid fstdx/fstdd file.");
    return false;
  }

  vector<char> header_compressed_byte(header_size_record.compressed_size);
  ins.seekg(-(record_size + header_size_record.compressed_size), std::ios::end);
  ins.read(const_cast<char *>(header_compressed_byte.data()),
           header_size_record.compressed_size);
  std::vector<char> header_json_raw_str;
  if (!decompress_to_buffer(
          header_compressed_byte.data(), header_compressed_byte.size(),
          header_size_record.original_size, header_json_raw_str)) {
    LOG_ERROR("It is not a valid fstdx/fstdd file.");
    return false;
  }
  try {
    header = json::parse(string(header_json_raw_str.data()));
    LOG_INFO("{}", header.dump());
  } catch (const json::exception &e) {
    LOG_ERROR("Parse header failed: {}", e.what());
    LOG_ERROR("header_json_raw_str: {}", string(header_json_raw_str.data()));
    return false;
  } catch (...) {
    LOG_ERROR("Parse header failed: unknown exception");
    return false;
  }
  return true;
}

} // namespace fstd