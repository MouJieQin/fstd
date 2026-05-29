#include <iostream>
#include <spdlog/cfg/env.h>
#include <string>
#include <vector>

#include "CLI/CLI.hpp"
#include "fstdx_searcher.hpp"
#include "fstdx_writer.hpp"
#include "logger.hpp"

#define VERSION "0.1.0"

using namespace std;
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

  bool search = false;
  app.add_flag("-s,--search", search, "进行精确匹配搜索");

  bool query_meta = false;
  app.add_flag("-m,--meta", query_meta, "查询元数据");

  std::string word;
  app.add_option("-w,--word", word, "指定输入文件路径");

  std::string file_path;
  app.add_option("file_path", file_path, "输入文件路径");

  // 4. 多值参数
  std::vector<int> nums;
  app.add_option("-n,--nums", nums, "传入一组数字");

  auto write_cmd = app.add_subcommand("write", "执行写操作");
  std::string input_file;
  write_cmd->add_option("-f,--file", input_file, "输入文件路径")->required();
  std::string delimiter = "</>";
  write_cmd->add_option("--delimiter", delimiter, "分隔符，默认</>")
      ->default_val("</>");
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
  write_cmd->add_option("-o,--output", output_file, "输出文件路径")
      ->default_val("output.fstdx");
  uint16_t block_size = 8;
  write_cmd
      ->add_option("-b,--block-size", block_size, "压缩块大小，默认8，单位KB")
      ->default_val("8");
  uint8_t compress_level = 5;
  write_cmd
      ->add_option("-l,--compress-level", compress_level, "压缩等级，默认5")
      ->default_val("5");
  uint16_t zstd_dict_size = 32;
  write_cmd
      ->add_option("--zstd-dict-size", zstd_dict_size,
                   "Zstd字典大小，默认32，单位KB")
      ->default_val("32");
  bool opt_sorted = false;
  write_cmd->add_flag("-s,--sorted", opt_sorted, "是否已按键排序，默认false");

  // 解析参数，异常自动捕获并打印帮助
  try {
    CLI11_PARSE(app, argc, argv);
  } catch (const CLI::ParseError &e) { return app.exit(e); }

  //   std::cout << "子命令：" << write_cmd->get_subcommand()->name() << "\n";
  std::cout << "输入文件：" << input_file << "\n";
  std::cout << "行索引键,值,步长列表：\n";
  std::cout << "输出文件：" << output_file << "\n";
  std::cout << "压缩块大小：" << block_size << " KB\n";
  std::cout << "压缩等级：" << static_cast<int>(compress_level) << "\n";
  std::cout << "Zstd字典大小：" << zstd_dict_size << " KB\n";
  std::cout << "是否已按键排序：" << (opt_sorted ? "是" : "否") << "\n";
  std::cout << "详细日志：" << (verbose ? "开启" : "关闭") << "\n";

  if (*write_cmd) {
    json meta_json;
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
    meta_json["Version"] = VERSION;

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

    fstd::FstdxWriter fstd_writer;
    int ret = fstd_writer.compile_fstdmx(input_file, output_file, delimiter,
                                         meta_json, block_size, compress_level,
                                         zstd_dict_size, opt_sorted, verbose);

    if (ret != 0) {
      LOG_ERROR("编译失败，返回码：{}", ret);
      return ret;
    }
    LOG_INFO("编译成功");
  } else if (query_meta) {
    LOG_INFO("开始搜索");
    bool is_valid = false;
    fstd::FstdxSearcher fstdx_searcher(file_path, is_valid);
    if (!is_valid) {
      LOG_ERROR("文件 {} 不是有效的 fstdx 文件", file_path);
      return 1;
    }
    json meta = fstdx_searcher.get_meta();
    std::cout << meta.dump(2) << std::endl;

  }

  else if (search) {
    LOG_INFO("开始搜索");
    bool is_valid = false;
    fstd::FstdxSearcher fstdx_searcher(file_path, is_valid);
    if (!is_valid) {
      LOG_ERROR("文件 {} 不是有效的 fstdx 文件", file_path);
      return 1;
    }
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

  return 0;
}