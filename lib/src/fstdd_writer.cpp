#include <fstd/common.h>
#include <fstd/fstdd_compressor.h>
#include <fstd/fstdd_writer.h>
#include <fstd/logger.h>

namespace fstd {
using namespace std;

int FstddWriter::compile_fstdd(const std::vector<std::string> &data_paths,
                               const std::string &output_file,
                               const nlohmann::json &meta, size_t block_size_kb,
                               size_t compress_level, size_t worker_num,
                               bool opt_verbose) {
  ofstream fout(output_file, ios_base::binary);
  if (!fout) {
    LOG_ERROR("Failed to open file {} for writing.", output_file);
    return 1;
  }
  DdJsonHeader header;
  if (!handle_meta(meta, meta_default, header)) { return 5; }
  FstddCompressor dd_compressor;
  if (!dd_compressor.compress(data_paths, fout, header, block_size_kb * 1024,
                              compress_level, worker_num, opt_verbose)) {
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

int FstddWriter::compile_fstdd(const string &data_path,
                               const string &output_file,
                               const nlohmann::json &meta, size_t block_size_kb,
                               size_t compress_level, size_t worker_num,
                               bool opt_verbose) {

  const std::vector<std::string> data_paths{data_path};
  return compile_fstdd(data_paths, output_file, meta, block_size_kb,
                       compress_level, worker_num, opt_verbose);
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