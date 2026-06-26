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
  content = std::string((std::istreambuf_iterator<char>(file_stream)),
                        std::istreambuf_iterator<char>());
  return true;
}

int main(int argc, char **argv) {
  Logger::instance(); // init logger
  spdlog::cfg::load_env_levels();
  CLI::App app{"fstd - a dictionary engine"};
  app.footer(" based on Finite State Transducer");

  bool verbose = false;
  app.add_flag("-v,--verbose", verbose, "enable verbose logging");

  CLI::App *extract_cmd = app.add_subcommand("extract", "extract fstdx/fstdd");

  std::string extract_input_file;
  extract_cmd->add_option("input", extract_input_file, "input file path")
      ->required();

  std::string key_file_path;
  extract_cmd->add_option("-k,--key-file-path", key_file_path, "key file path");

  std::string extract_output_file;
  extract_cmd->add_option("-o,--output", extract_output_file, "output file path")
      ->default_val(change_ext(extract_input_file, "txt"));

  CLI::App *search_cmd = app.add_subcommand("search", "search words");

  // bool search_word = false;
  // search_cmd->add_option("-s,--search", search_word, "exact match search");

  bool common_prefix = false;
  search_cmd->add_flag("-c,--common-prefix", common_prefix, "search common prefix");

  bool longest_common_prefix = false;
  search_cmd->add_flag("-l,--longest-common-prefix", longest_common_prefix,
                       "search longest common prefix");

  bool predictive = false;
  search_cmd->add_flag("-p,--predictive", predictive, "predictive search");

  bool edit_distance = false;
  search_cmd->add_flag("-e,--edit-distance", edit_distance, "edit distance");

  bool regex = false;
  search_cmd->add_flag("-r,--regex", regex, "regex search");

  bool spellcheck = false;
  search_cmd->add_flag("-s,--spellcheck", spellcheck, "spellcheck word");

  bool enumerate = false;
  search_cmd->add_flag("-u,--enumerate", enumerate, "enumerate all words");

  bool query_meta = false;
  search_cmd->add_flag("-m,--meta", query_meta, "query metadata");

  std::string file_path;
  search_cmd->add_option("file_path", file_path, "input file path")->required();

  std::string word;
  search_cmd->add_option("word", word, "word");


  CLI::App *write_cmd = app.add_subcommand("write", "write words to fstdx/fstdd");
  std::string write_input_file;
  write_cmd->add_option("-f,--file", write_input_file, "input file path")
      ->required();
  std::string title = "";
  write_cmd->add_option("-t,--title", title, "title, [string/file]")
      ->default_val("");
  std::string description = "";
  write_cmd
      ->add_option("-d,--description", description, "description, [string/file]")
      ->default_val("");
  std::string meta_json_file = "";
  write_cmd->add_option("-m,--meta", meta_json_file, "JSON metadata file path")
      ->default_val("");
  std::string output_file;
  write_cmd->add_option("-o,--output", output_file, "output file path");

  uint16_t block_size = 8;
  write_cmd
      ->add_option("-b,--block-size", block_size, "default block size 8, unit KB")
      ->default_val("8");
  uint8_t compress_level = 5;
  write_cmd
      ->add_option("-l,--compress-level", compress_level, "default compress level 5")
      ->default_val("5");
  uint16_t zstd_dict_size = 100;
  write_cmd
      ->add_option("--zstd-dict-size", zstd_dict_size,
                   "default zstd dict size 100, unit KB")
      ->default_val("32");
  bool opt_sorted = false;
  write_cmd->add_flag("-s,--sorted", opt_sorted, "default sorted false");

  size_t write_worker_num = 0;
  write_cmd
      ->add_option("-w,--worker", write_worker_num,
                   "the number of thread workers.")
      ->default_val(0);

  try {
    CLI11_PARSE(app, argc, argv);
  } catch (const CLI::ParseError &e) { return app.exit(e); }

  std::cout << "input file path：" << write_input_file << "\n";
  std::cout << "index, value, step list：\n";
  std::cout << "output file path：" << output_file << "\n";
  std::cout << "default block size：" << block_size << " KB\n";
  std::cout << "default compress level：" << static_cast<int>(compress_level) << "\n";
  std::cout << "default zstd dict size：" << zstd_dict_size << " KB\n";
  std::cout << "default sorted false：" << (opt_sorted ? "true" : "false") << "\n";
  std::cout << "verbose：" << (verbose ? "true" : "false") << "\n";

  if (*search_cmd) {
    if (ends_with(file_path, ".fstdd")) {
      fstd::FstddReader fstdd_reader(file_path);
      if (!fstdd_reader) {
        LOG_ERROR("file {} is not a valid fstdd file", file_path);
        return 1;
      }
      if (query_meta) {
        json meta = fstdd_reader.get_meta();
        std::cout << meta.dump(2) << std::endl;
      }
    } else if (ends_with(file_path, ".fstdx")) {
      fstd::FstdxReader fstdx_searcher(file_path);
      if (!fstdx_searcher) {
        LOG_ERROR("file {} is not a valid fstdx file", file_path);
        return 1;
      }

      if (query_meta) {
        json meta = fstdx_searcher.get_meta();
        std::cout << meta.dump(2) << std::endl;
      } else if (common_prefix) {
        LOG_INFO("search common prefix");
        auto result = fstdx_searcher.common_prefix_search(word);
        if (result.empty()) {
          LOG_INFO("no match found");
          return 1;
        }
        for (const auto &p : result) {
          std::cout << *p << std::endl;
        }
      } else if (longest_common_prefix) {
        LOG_INFO("search longest common prefix");
        std::pair<std::string, uint64_t> result;
        size_t len = fstdx_searcher.longest_prefix_len(word);
        if (len == 0) {
          LOG_INFO("no match found");
          return 1;
        }
        std::cout << "longest common prefix: " << result.first << std::endl;
        std::cout << "longest common prefix length: " << len << std::endl;
      } else if (predictive) {
        LOG_INFO("predictive search");
        auto result = fstdx_searcher.predictive_search(word);
        if (result.empty()) {
          LOG_INFO("no match found");
          return 1;
        }
        for (const auto &p : result) {
          std::cout << *p << std::endl;
        }
      } else if (edit_distance) {
        LOG_INFO("edit distance search");
        auto result = fstdx_searcher.edit_distance_search(word, 1);
        if (result.empty()) {
          LOG_INFO("no match found");
          return 1;
        }
        for (const auto &p : result) {
          std::cout << *p << std::endl;
        }
      } else if (regex) {
        LOG_INFO("regex search");
        auto p_results =
            fstdx_searcher.regex_search(word);
        const auto &results = p_results.first;
        const auto &error_message = p_results.second;
        if (!error_message.empty()) {
          LOG_ERROR("regex error: {}", error_message);
          return 1;
        }
        if (results.empty()) {
          LOG_INFO("no match found");
          return 1;
        }
        for (const auto &p : results) {
          std::cout << *p << std::endl;
        }
      } else if (spellcheck) {
        LOG_INFO("spellcheck search");
        // std::vector<std::tuple<double, std::string, uint64_t>> result =
        //     fstdx_searcher.spellcheck_word(word);
        // if (result.empty()) {
        //   LOG_INFO("no match found");
        //   return 1;
        // }
        // for (const auto &p : result) {
        //   std::cout << std::get<1>(p) << " -> " << std::get<2>(p) << std::endl;
        // }
      } else if (enumerate) {
        LOG_INFO("enumerate all keys");
        // auto result = fstdx_searcher.enumerate();
        // if (result.empty()) {
        //   LOG_INFO("no match found");
        //   return 1;
        // }
        // for (const auto &p : result) {
        //   std::cout << *p << std::endl;
        // }
      } else {
        LOG_INFO("exact match search");
        std::vector<std::string> result;
        bool res = fstdx_searcher.exact_match_search(word, result);
        if (!res) {
          LOG_INFO("no match found");
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
        LOG_ERROR("file {} not found", meta_json_file);
        return 1;
      } else {
        try {
          meta_json = json::parse(content);
        } catch (const json::exception &e) {
          LOG_ERROR("file {} format error: {}", meta_json_file, e.what());
          return 1;
        } catch (const std::exception &e) {
          LOG_ERROR("file {} read error: {}", meta_json_file, e.what());
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
      int ret = fstdd_writer.compile_fstdd(
          write_input_file, output_file, meta_json, block_size, compress_level,
          write_worker_num, verbose);
      if (ret != 0) {
        LOG_ERROR("compile failed, return code: {}", ret);
        return ret;
      }
    } else if (std::filesystem::is_regular_file(write_input_file)) {
      if (output_file.empty()) { output_file = "output.fstdx"; }
      fstd::FstdxWriter fstdx_writer;
      int ret = fstdx_writer.compile_fstdx(
          write_input_file, output_file, meta_json, block_size, compress_level,
          zstd_dict_size, write_worker_num, opt_sorted, verbose);

      if (ret != 0) {
        LOG_ERROR("compile failed, return code: {}", ret);
        return ret;
      }
    } else {
      LOG_ERROR("Only support directory/file");
      return 1;
    }
    LOG_INFO("compile success");
  } else if (*extract_cmd) {

    if (ends_with(extract_input_file, ".fstdx")) {
      fstd::FstdxReader fstdx_reader(extract_input_file);
      if (!fstdx_reader) { return 1; }
      bool ret = fstdx_reader.extract(extract_output_file);
      if (!ret) { return 3; }
    } else if (ends_with(extract_input_file, ".fstdd")) {
      fstd::FstddReader fstdd_reader(extract_input_file);
      if (!fstdd_reader) { return 1; }
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
    LOG_ERROR("unknown subcommand");
    return 1;
  }
  return 0;
}