#include <fstd/common.h>
#include <fstd/fstdd_writer.h>
#include <fstd/logger.h>

namespace fstd {
using namespace std;

int FstddWriter::compile_fstdd(const std::vector<std::string> &data_paths,
                               const std::string &output_file,
                               const std::string &meta_json_str,
                               size_t block_size_kb, size_t compress_level,
                               size_t worker_num, bool opt_verbose,
                               size_t file_stream_num) {

  using json = nlohmann::json;
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
  return compile_fstdd(data_paths, output_file, meta_json, block_size_kb,
                       compress_level, worker_num, opt_verbose,
                       file_stream_num);
}

int FstddWriter::compile_fstdd(const std::vector<std::string> &data_paths,
                               const std::string &output_file,
                               const nlohmann::json &meta, size_t block_size_kb,
                               size_t compress_level, size_t worker_num,
                               bool opt_verbose, size_t file_stream_num) {
  std::filesystem::path path_obj(
      reinterpret_cast<const char8_t *>(output_file.c_str()));
  ofstream fout(path_obj, ios_base::binary);
  if (!fout) {
    LOG_ERROR("Failed to open file {} for writing.", path_obj.string());
    return 1;
  }
  DdJsonHeader header;
  if (!handle_meta(meta, meta_default, header)) { return 5; }
  if (!dd_compressor.compress(data_paths, fout, header, block_size_kb * 1024,
                              compress_level, worker_num, opt_verbose,
                              file_stream_num)) {
    return 2;
  }
  header["meta"]["Creationdate"] = get_current_date();
  header["meta"]["Compressionlevel"] = compress_level;
  std::vector<char> comp_header_dst;
  std::string header_str = header.dump();
  {
    bool comp_res =
        compress_to_buffer(header_str.c_str(), header_str.size() + 1,
                           comp_header_dst, compress_level);
    if (!comp_res) { return 4; }
  }
  fout.write(comp_header_dst.data(), comp_header_dst.size());
  HeaderSizeRecord header_size_record(header_str.size() + 1,
                                      comp_header_dst.size());
  LOG_INFO("{}", header.dump(2));
  LOG_DEBUG("{},{}", header_size_record.original_size,
            header_size_record.compressed_size);
  fout.write(reinterpret_cast<const char *>(&header_size_record),
             sizeof(HeaderSizeRecord));
  return 0;
}

int FstddWriter::compile_fstdd(const std::string &data_path,
                               const std::string &output_file,
                               const std::string &meta_json_str,
                               size_t block_size_kb, size_t compress_level,
                               size_t worker_num, bool opt_verbose) {
  using json = nlohmann::json;
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
  return compile_fstdd(data_path, output_file, meta_json, block_size_kb,
                       compress_level, worker_num, opt_verbose);
}

int FstddWriter::compile_fstdd(const string &data_path,
                               const string &output_file,
                               const nlohmann::json &meta, size_t block_size_kb,
                               size_t compress_level, size_t worker_num,
                               bool opt_verbose) {

  const std::vector<std::string> data_paths{data_path};
  return compile_fstdd(data_paths, output_file, meta, block_size_kb,
                       compress_level, worker_num, opt_verbose);
}

bool FstddWriter::push_file_stream(const std::string &file_path,
                                   std::string_view stream) {
  return dd_compressor.push_file_stream(file_path, stream);
}

const nlohmann::json FstddWriter::meta_default =
    nlohmann::json::array({{{"Version", FSTD_VERSION}},
                           {{"Record", 0}},
                           {{"Format", ""}},
                           {{"Keycasesensitive", true}},
                           {{"Stripkey", false}},
                           {{"Description", ""}},
                           {{"Title", ""}},
                           {{"Encoding", "UTF-8"}},
                           {{"Creationdate", ""}},
                           {{"Compressionlevel", 3}}});
} // namespace fstd