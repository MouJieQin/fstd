#include <iostream>
#include <spdlog/cfg/env.h>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>
#include <fstd/fstdd_reader.h>
#include <fstd/fstdd_writer.h>
#include <fstd/fstdx_reader.h>
#include <fstd/fstdx_writer.h>
#include <fstd/logger.h>

using namespace std;
using namespace fstd;
using json = nlohmann::json;

bool read_file(const std::string &file_path, std::string &content) {
  std::ifstream file_stream(file_path);
  if (!file_stream) { return false; }
  // 一行读完
  content = std::string((std::istreambuf_iterator<char>(file_stream)),
                        std::istreambuf_iterator<char>());
  return true;
}

int main(int argc, char **argv) {
  Logger::instance(); // init logger
  spdlog::cfg::load_env_levels();
  CLI::App app{"fstd - 命令行参数解析示例"};
  app.footer("示例程序 - 基于CLI11");

  // // 1. 普通字符串参数
  // std::string file_path;
  // app.add_option("-f,--file", file_path, "指定输入文件路径");

  // // 2. 整型参数，带默认值
  // int count = 10;
  // app.add_option("-c,--count", count, "处理数量")->default_val("10");

  // 3. 布尔开关
  bool verbose = false;
  app.add_flag("-v,--verbose", verbose, "开启详细日志输出");

  CLI::App *extract_cmd = app.add_subcommand("extract", "extract fstdx/fstdd");

  std::string extract_input_file;
  extract_cmd->add_option("input", extract_input_file, "输入文件路径")
      ->required();

  std::string key_file_path;
  extract_cmd->add_option("-k,--key-file-path", key_file_path, "输入文件路径");

  std::string extract_output_file;
  extract_cmd->add_option("-o,--output", extract_output_file, "输入文件路径")
      ->default_val(change_ext(extract_input_file, "txt"));

  CLI::App *search_cmd = app.add_subcommand("search", "执行搜索操作");

  // bool search_word = false;
  // search_cmd->add_option("-s,--search", search_word, "进行精确匹配搜索");

  bool common_prefix = false;
  search_cmd->add_flag("-c,--common-prefix", common_prefix, "搜索公共前缀");

  bool longest_common_prefix = false;
  search_cmd->add_flag("-l,--longest-common-prefix", longest_common_prefix,
                       "搜索最长公前缀");

  bool predictive = false;
  search_cmd->add_flag("-p,--predictive", predictive, "指定预测词");

  bool edit_distance = false;
  search_cmd->add_flag("-e,--edit-distance", edit_distance, "指定编辑距离");

  bool regex = false;
  search_cmd->add_flag("-r,--regex", regex, "指定正则表达式");

  bool spellcheck = false;
  search_cmd->add_flag("-s,--spellcheck", spellcheck, "指定拼写检查");

  bool enumerate = false;
  search_cmd->add_flag("-u,--enumerate", enumerate, "枚举所有单词");

  bool query_meta = false;
  search_cmd->add_flag("-m,--meta", query_meta, "查询元数据");

  std::string file_path;
  search_cmd->add_option("file_path", file_path, "输入文件路径")->required();

  std::string word;
  search_cmd->add_option("word", word, "输入单词");

  // // 4. 多值参数
  // std::vector<int> nums;
  // app.add_option("-n,--nums", nums, "传入一组数字");

  CLI::App *write_cmd = app.add_subcommand("write", "执行写操作");
  std::string write_input_file;
  write_cmd->add_option("-f,--file", write_input_file, "输入文件路径")
      ->required();
  std::string title = "";
  write_cmd->add_option("-t,--title", title, "标题，[字符串/文件路径]")
      ->default_val("");
  std::string description = "";
  write_cmd
      ->add_option("-d,--description", description, "描述，[字符串/文件路径]")
      ->default_val("");
  std::string meta_json_file = "";
  write_cmd->add_option("-m,--meta", meta_json_file, "JSON元数据文件路径")
      ->default_val("");
  std::string output_file;
  write_cmd->add_option("-o,--output", output_file, "输出文件路径");

  uint16_t block_size = 8;
  write_cmd
      ->add_option("-b,--block-size", block_size, "压缩块大小，默认8，单位KB")
      ->default_val("8");
  uint8_t compress_level = 5;
  write_cmd
      ->add_option("-l,--compress-level", compress_level, "压缩等级，默认5")
      ->default_val("5");
  uint16_t zstd_dict_size = 100;
  write_cmd
      ->add_option("--zstd-dict-size", zstd_dict_size,
                   "Zstd字典大小，默认100，单位KB")
      ->default_val("32");
  bool opt_sorted = false;
  write_cmd->add_flag("-s,--sorted", opt_sorted, "是否已按键排序，默认false");

  size_t write_worker_num = 0;
  write_cmd
      ->add_option("-w,--worker", write_worker_num,
                   "the number of thread workers.")
      ->default_val(0);

  // 解析参数，异常自动捕获并打印帮助
  try {
    CLI11_PARSE(app, argc, argv);
  } catch (const CLI::ParseError &e) { return app.exit(e); }

  //   std::cout << "子命令：" << write_cmd->get_subcommand()->name() << "\n";
  std::cout << "输入文件：" << write_input_file << "\n";
  std::cout << "行索引键,值,步长列表：\n";
  std::cout << "输出文件：" << output_file << "\n";
  std::cout << "压缩块大小：" << block_size << " KB\n";
  std::cout << "压缩等级：" << static_cast<int>(compress_level) << "\n";
  std::cout << "Zstd字典大小：" << zstd_dict_size << " KB\n";
  std::cout << "是否已按键排序：" << (opt_sorted ? "是" : "否") << "\n";
  std::cout << "详细日志：" << (verbose ? "开启" : "关闭") << "\n";

  if (*search_cmd) {
    bool is_valid = false;
    if (ends_with(file_path, ".fstdd")) {
      fstd::FstddReader fstdd_reader(file_path, is_valid);
      if (!is_valid) {
        LOG_ERROR("文件 {} 不是有效的 fstdd 文件", file_path);
        return 1;
      }
      if (query_meta) {
        json meta = fstdd_reader.get_meta();
        std::cout << meta.dump(2) << std::endl;
      }
    } else if (ends_with(file_path, ".fstdx")) {
      fstd::FstdxReader fstdx_searcher(file_path, is_valid);
      if (!is_valid) {
        LOG_ERROR("文件 {} 不是有效的 fstdx 文件", file_path);
        return 1;
      }

      if (query_meta) {
        json meta = fstdx_searcher.get_meta();
        std::cout << meta.dump(2) << std::endl;
      } else if (common_prefix) {
        LOG_INFO("搜索公共前缀");
        std::vector<std::pair<std::string, uint64_t>> result =
            fstdx_searcher.common_prefix_search(word);
        if (result.empty()) {
          LOG_INFO("未找到匹配项");
          return 1;
        }
        for (const auto &p : result) {
          std::cout << p.first << " -> " << p.second << std::endl;
        }
      } else if (longest_common_prefix) {
        LOG_INFO("搜索最长公前缀");
        std::pair<std::string, uint64_t> result;
        size_t len = fstdx_searcher.longest_common_prefix_search(word, result);
        if (len == 0) {
          LOG_INFO("未找到匹配项");
          return 1;
        }
        std::cout << "最长公前缀: " << result.first << std::endl;
        std::cout << "最长公前缀长度: " << len << std::endl;
      } else if (predictive) {
        LOG_INFO("指定预测词");
        std::vector<std::pair<std::string, uint64_t>> result =
            fstdx_searcher.predictive_search(word);
        if (result.empty()) {
          LOG_INFO("未找到匹配项");
          return 1;
        }
        for (const auto &p : result) {
          std::cout << p.first << " -> " << p.second << std::endl;
        }
      } else if (edit_distance) {
        LOG_INFO("指定编辑距离");
        std::vector<std::pair<std::string, uint64_t>> result =
            fstdx_searcher.edit_distance_search(word, 1);
        if (result.empty()) {
          LOG_INFO("未找到匹配项");
          return 1;
        }
        for (const auto &p : result) {
          std::cout << p.first << " -> " << p.second << std::endl;
        }
      } else if (regex) {
        LOG_INFO("指定正则表达式");
        std::pair<std::vector<std::pair<std::string, uint64_t>>, std::string>
            p_results = fstdx_searcher.regex_search(word);
        const auto &results = p_results.first;
        const auto &error_message = p_results.second;
        if (!error_message.empty()) {
          LOG_ERROR("正则表达式错误：{}", error_message);
          return 1;
        }
        if (results.empty()) {
          LOG_INFO("未找到匹配项");
          return 1;
        }
        for (const auto &p : results) {
          std::cout << p.first << " -> " << p.second << std::endl;
        }
      } else if (spellcheck) {
        LOG_INFO("指定拼写检查");
        std::vector<std::tuple<double, std::string, uint64_t>> result =
            fstdx_searcher.spellcheck_word(word);
        if (result.empty()) {
          LOG_INFO("未找到匹配项");
          return 1;
        }
        for (const auto &p : result) {
          std::cout << std::get<1>(p) << " -> " << std::get<2>(p) << std::endl;
        }
      } else if (enumerate) {
        LOG_INFO("枚举所有键");
        std::vector<std::pair<std::string, uint64_t>> result =
            fstdx_searcher.enumerate();
        if (result.empty()) {
          LOG_INFO("未找到匹配项");
          return 1;
        }
        for (const auto &p : result) {
          std::cout << p.first << " -> " << p.second << std::endl;
        }
      } else {
        LOG_INFO("进行精确匹配搜索");
        std::vector<std::string> result;
        bool res = fstdx_searcher.exact_match_search(word, result);
        if (!res) {
          LOG_INFO("未找到匹配项");
          return 1;
        }
        for (const std::string &s : result) {
          std::cout << "------------------------------" << std::endl;
          std::cout << s << std::endl;
          std::cout << "------------------------------" << std::endl;
        }
      }
    }
  } else if (*write_cmd) {
    json meta_json;
    meta_json["Version"] = FSTD_VERSION;
    if (!meta_json_file.empty()) {
      std::string content;
      if (!read_file(meta_json_file, content)) {
        LOG_ERROR("文件 {} 不存在", meta_json_file);
        return 1;
      } else {
        try {
          meta_json = json::parse(content);
        } catch (const json::exception &e) {
          LOG_ERROR("JSON元数据文件 {} 格式错误：{}", meta_json_file, e.what());
          return 1;
        } catch (const std::exception &e) {
          LOG_ERROR("JSON元数据文件 {} 读取错误：{}", meta_json_file, e.what());
          return 1;
        }
      }
    }

    if (!title.empty()) {
      string content;
      if (read_file(title, content)) {
        meta_json["Title"] = content;
      } else {
        meta_json["Title"] = title;
      }
    }

    if (!description.empty()) {
      string content;
      if (read_file(description, content)) {
        meta_json["Description"] = content;
      } else {
        meta_json["Description"] = description;
      }
    }

    if (!description.empty()) { meta_json["description"] = description; }

    if (std::filesystem::is_directory(write_input_file)) {
      if (output_file.empty()) { output_file = "output.fstdd"; }
      fstd::FstddWriter fstdd_writer;
      int ret =
          fstdd_writer.compile_fstdd(write_input_file, output_file, meta_json,
                                     compress_level, write_worker_num, verbose);
      if (ret != 0) {
        LOG_ERROR("编译失败，返回码：{}", ret);
        return ret;
      }
    } else if (std::filesystem::is_regular_file(write_input_file)) {
      if (output_file.empty()) { output_file = "output.fstdx"; }
      fstd::FstdxWriter fstdx_writer;
      int ret = fstdx_writer.compile_fstdx(
          write_input_file, output_file, meta_json, block_size, compress_level,
          zstd_dict_size, write_worker_num, opt_sorted, verbose);

      if (ret != 0) {
        LOG_ERROR("编译失败，返回码：{}", ret);
        return ret;
      }
    } else {
      LOG_ERROR("Only support directory/file");
      return 1;
    }
    LOG_INFO("编译成功");
  } else if (*extract_cmd) {

    if (ends_with(extract_input_file, ".fstdx")) {
      bool is_valid = true;
      fstd::FstdxReader fstdx_reader(extract_input_file, is_valid);
      if (!is_valid) { return 1; }
      bool ret = fstdx_reader.extract(extract_output_file);
      if (!ret) { return 3; }
    } else if (ends_with(extract_input_file, ".fstdd")) {
      bool is_valid = true;
      fstd::FstddReader fstdd_reader(extract_input_file, is_valid);
      if (!is_valid) { return 1; }
      if (key_file_path.empty()) {
        if (!fstdd_reader.extract_all()) { return 1; }
      } else {
        if (!fstdd_reader.extract(key_file_path)) { return 1; }
      }
    } else {
      LOG_ERROR("Only support fstdd/fstdx");
      return 1;
    }
  } else {
    LOG_ERROR("未知子命令");
    return 1;
  }
  return 0;
}