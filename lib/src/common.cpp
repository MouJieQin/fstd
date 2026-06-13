#include <zstd.h>

#include <fstd/common.h>
namespace fstd {

using namespace std;
using json = nlohmann::json;

HeaderSizeRecord::HeaderSizeRecord(uint32_t original_size,
                                   uint32_t compressed_size)
    : original_size(original_size), compressed_size(compressed_size) {}

std::string get_current_date() {
  std::time_t now = std::time(nullptr);
  std::tm local_tm = *std::localtime(&now);
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d", local_tm.tm_year + 1900,
                local_tm.tm_mon + 1, local_tm.tm_mday);
  return std::string(buf);
}

bool handle_meta(const nlohmann::json &meta, const nlohmann::json &meta_default,
                 nlohmann::json &header) {
  if (!meta.is_object()) {
    LOG_ERROR("Meta is not an object.");
    return false;
  }

  json &h_meta = header["meta"];
  // 遍历 meta_default
  for (const auto &item : meta_default) {
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
    LOG_ERROR("压缩失败: {}", ZSTD_getErrorName(comp_size));
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
    LOG_ERROR("Decompression failed: {}", ZSTD_getErrorName(actual_decomp_size));
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
  if (file_size < header_size_record.compressed_size) {
    LOG_ERROR("It is not a valid fstdx/fstdd file.");
    return false;
  }

  vector<char> header_compressed_byte(header_size_record.compressed_size);
  ins.seekg(-(record_size + header_size_record.compressed_size), std::ios::end);
  ins.read(const_cast<char *>(header_compressed_byte.data()),
           header_size_record.compressed_size);
  std::vector<char> header_json_raw_str;
  decompress_to_buffer(header_compressed_byte.data(),
                       header_compressed_byte.size(),
                       header_size_record.original_size, header_json_raw_str);
  try {
    header = json::parse(string(header_json_raw_str.data()));
    LOG_INFO("{}", header.dump());
  } catch (const json::exception &e) {
    LOG_ERROR("解析 header 失败: {}", e.what());
    return false;
  } catch (...) {
    LOG_ERROR("解析 header 失败: 未知异常");
    return false;
  }
  return true;
}

} // namespace fstd