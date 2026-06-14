#include "fstd/fstdx_reader.h"
#include "fstd/fstdx_writer.h"
#include "fstd/logger.h"
#include <fstream>
#include <gtest/gtest.h>
#include <string>

// int FstdxWriter::compile_fstdx(const std::string &input_file,
//                                const std::string &output_file, const json
//                                &meta, uint16_t block_size_kb, uint8_t
//                                compress_level, uint16_t zstd_dict_size_kb,
//                                size_t worker_num, bool opt_sorted, bool
//                                opt_verbose)
using namespace std;
using namespace fstd;
using json = nlohmann::json;
namespace fs = std::filesystem;
// 测试示例：你可以随便写

const string fstdx_out_path = string(TEST_DATA_DIR) + "/dict.fstdx";
const string raw_file_path = string(TEST_DATA_DIR) + "/dict.txt";

TEST(FstdxWriteTest, CompileTest) {
  FstdxWriter writer;
  json meta = {{"Title", "dict"}};
  ASSERT_TRUE(writer.compile_fstdx(raw_file_path, fstdx_out_path, meta, 8, 5, 4,
                                   0, false, true) == 0);
}

TEST(FstdxWriteTest, SearchTest) {
  FstdxWriter writer;
  vector<string> keys, values;
  ASSERT_TRUE(writer.load_file(raw_file_path, keys, values));
  ASSERT_EQ(keys.size(), values.size());
  bool is_valid = false;
  FstdxReader reader(fstdx_out_path, is_valid);
  ASSERT_TRUE(is_valid);
  for (size_t i = 0; i < keys.size(); i++) {
    vector<string> result;
    ASSERT_TRUE(reader.exact_match_search(keys[i], result));
    for (size_t j = 0; j < result.size(); j++) {
      ASSERT_EQ(result[j], values[i + j]);
    }
  }
}