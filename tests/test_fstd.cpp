#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_TRACE
#include "fstd/fstdd_reader.h"
#include "fstd/fstdd_writer.h"
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

const string lib_dir = string(TEST_DATA_DIR) + "/../lib";
const string cache_dir = string(TEST_DATA_DIR) + "/cache";
const string dict_dir = string(TEST_DATA_DIR) + "/dict";
const string fstdx_out_path = string(TEST_DATA_DIR) + "/dict/dict.fstdx";
const string fstdd_out_path = string(TEST_DATA_DIR) + "/dict.fstdd";
const string raw_file_path = string(TEST_DATA_DIR) + "/dict/dict.txt";
const string extract_file_path = cache_dir + "/extract_dict.txt";
const vector<string> data_paths{lib_dir, dict_dir};

TEST(FstdxWriteTest, CompileTest) {
  LOG_INFO("Init logger for loading environment variables ...");
  spdlog::cfg::load_env_levels();
  FstdxWriter writer;
  json meta = {{"Title", "dict"}};
  ASSERT_TRUE(writer.compile_fstdx(raw_file_path, fstdx_out_path, meta, 8, 22,
                                   4, 0, false, true) == 0);
}

TEST(FstdxReadTest, SearchTest) {
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

TEST(FstdxReadTest, ExtractTest) {
  bool is_valid = false;
  FstdxReader reader(fstdx_out_path, is_valid);
  ASSERT_TRUE(is_valid);
  ASSERT_TRUE(reader.extract(extract_file_path));
  FstdxWriter writer;
  vector<string> ex_keys, ex_values;
  ASSERT_TRUE(writer.load_file(extract_file_path, ex_keys, ex_values));
  vector<string> keys, values;
  ASSERT_TRUE(writer.load_file(raw_file_path, keys, values));
  ASSERT_EQ(keys.size(), ex_keys.size());
  ASSERT_EQ(values.size(), ex_values.size());
  auto sorted_ex_idxes = sort_indexes(ex_keys);
  auto sorted_idxes = sort_indexes(keys);
  for (size_t i = 0; i < sorted_idxes.size(); ++i) {
    ASSERT_EQ(keys[sorted_idxes[i]], ex_keys[sorted_ex_idxes[i]]);
    ASSERT_EQ(values[sorted_idxes[i]], ex_values[sorted_ex_idxes[i]]);
  }
}

TEST(FstddWriteTest, CompileTest) {
  json meta = {{"Title", "dict"}};
  FstddWriter writer;
  ASSERT_EQ(
      0, writer.compile_fstdd(data_paths, fstdd_out_path, meta, 10, 0, true));
}

bool are_files_equal_fast(const std::string &file1, const std::string &file2) {
  std::ifstream f1(file1, std::ios::binary | std::ios::ate);
  std::ifstream f2(file2, std::ios::binary | std::ios::ate);

  if (!f1 || !f2) {
    LOG_ERROR("Failed to open file {} or {}", file1, file2);
    return false;
  }
  if (f1.tellg() != f2.tellg()) {
    LOG_ERROR("File {} and {} have different sizes", file1, file2);
    return false;
  }

  f1.seekg(0);
  f2.seekg(0);

  const int BUF_SIZE = 4096;
  char buf1[BUF_SIZE], buf2[BUF_SIZE];

  while (f1.read(buf1, BUF_SIZE) && f2.read(buf2, BUF_SIZE)) {
    if (std::memcmp(buf1, buf2, BUF_SIZE) != 0) {
      LOG_ERROR("File {} and {} have different content", file1, file2);
      return false;
    }
  }
  return true;
}

TEST(FstddWriteTest, ReadTest) {
  bool is_valid = false;
  FstddReader reader(fstdd_out_path, is_valid);
  ASSERT_TRUE(is_valid);
  vector<pair<string, size_t>> files_paths =
      FstddCompressor::recursive_directory(data_paths);
  for (size_t i = 0; i < files_paths.size(); ++i) {
    const string &file_path = files_paths[i].first;
    const size_t dir_idx = files_paths[i].second;
    const string extract_dir = cache_dir + "/extract";
    ASSERT_TRUE(reader.extract(file_path, extract_dir));
    fs::path original_path = fs::path(data_paths[dir_idx]) / file_path;
    fs::path extract_path = fs::path(extract_dir) / file_path;
    ASSERT_TRUE(
        are_files_equal_fast(original_path.string(), extract_path.string()));
  }
}

TEST(FstddWriteTest, ExtractAllTest) {
  bool is_valid = false;
  FstddReader reader(fstdd_out_path, is_valid);
  ASSERT_TRUE(is_valid);
  const string extract_dir = cache_dir + "/extract_all";
  ASSERT_TRUE(reader.extract_all(extract_dir));
  vector<pair<string, size_t>> files_paths =
      FstddCompressor::recursive_directory(data_paths);
  for (size_t i = 0; i < files_paths.size(); ++i) {
    const string &file_path = files_paths[i].first;
    const size_t dir_idx = files_paths[i].second;
    fs::path original_path = fs::path(data_paths[dir_idx]) / file_path;
    fs::path extract_path = fs::path(extract_dir) / file_path;
    ASSERT_TRUE(
        are_files_equal_fast(original_path.string(), extract_path.string()));
  }
}