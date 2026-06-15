#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include "fstd/fstdx_reader.h"
#include "fstd/fstdx_writer.h"
#include "fstd/logger.h"
#include <fstream>
#include <gtest/gtest.h>
#include <spdlog/cfg/env.h>
#include <string>

using namespace std;
using namespace fstd;
using json = nlohmann::json;
namespace fs = std::filesystem;

const string fstdx_out_path = string(TEST_DATA_DIR) + "/dict.fstdx";
const string raw_file_path = string(TEST_DATA_DIR) + "/dict.txt";

TEST(FstdxWriteTest, CompileTest) {
  LOG_INFO("Init logger for loading environment variables ...");
  spdlog::cfg::load_env_levels();
  FstdxWriter writer;
  json meta = {{"Title", "dict"}};
  ASSERT_TRUE(writer.compile_fstdx(raw_file_path, fstdx_out_path, meta, 8, 22, 4,
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