#include <iostream>
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
namespace fs = std::filesystem;

int cli_app(int argc, char **argv) {
  CLI::App app{"fstd - a dictionary engine"};
  app.footer(" based on Finite State Transducer");

  bool verbose = false;
  app.add_flag("--verbose", verbose, "enable verbose logging");

  bool version = false;
  app.add_flag("-v,--version", version, "show version");

  uint8_t log_level = 4;
  app.add_option("-l,--log-level", log_level, "log level [0-6]")
      ->default_val(4);

  //-----------------------------------------------------------------------------
  //   extract
  //-----------------------------------------------------------------------------
  CLI::App *extract_cmd = app.add_subcommand("extract", "extract fstdx/fstdd");

  std::string extract_input_file;
  extract_cmd->add_option("input", extract_input_file, "input file path")
      ->required();

  std::string key_file_path;
  extract_cmd->add_option("-k,--key-file-path", key_file_path, "key file path");

  std::string extract_output_path;
  extract_cmd->add_option("-o,--output", extract_output_path,
                          "output file path");

  //-----------------------------------------------------------------------------
  //   search
  //-----------------------------------------------------------------------------
  CLI::App *search_cmd = app.add_subcommand("search", "search words");

  // bool search_word = false;
  // search_cmd->add_option("-s,--search", search_word, "exact match search");

  bool contains = false;
  search_cmd->add_flag("-c,--contains", contains, "contains key");

  bool predictive = false;
  search_cmd->add_flag("-p,--predictive", predictive, "predictive search");

  bool edit_distance = false;
  search_cmd->add_flag("-e,--edit-distance", edit_distance, "edit distance");

  bool regex = false;
  search_cmd->add_flag("-r,--regex", regex, "regex search");

  bool spellcheck = false;
  search_cmd->add_flag("-s,--spellcheck", spellcheck, "spellcheck word");

  bool common_prefix = false;
  search_cmd->add_flag("--common-prefix", common_prefix,
                       "search common prefix");

  bool longest_common_prefix = false;
  search_cmd->add_flag("-l,--longest-common-prefix", longest_common_prefix,
                       "search longest common prefix");

  bool enumerate = false;
  search_cmd->add_flag("-u,--enumerate", enumerate, "enumerate all words");

  bool query_meta = false;
  search_cmd->add_flag("-m,--meta", query_meta, "query metadata");

  std::string file_path;
  search_cmd->add_option("file_path", file_path, "input file path")->required();

  std::string word;
  search_cmd->add_option("word", word, "word");

  //-----------------------------------------------------------------------------
  //   write
  //-----------------------------------------------------------------------------
  CLI::App *write_cmd =
      app.add_subcommand("write", "write words to fstdx/fstdd");
  std::string write_input_file;
  write_cmd->add_option("-f,--file", write_input_file, "input file path")
      ->required();
  std::string title = "";
  write_cmd->add_option("-t,--title", title, "title, [string/file]")
      ->default_val("");
  std::string description = "";
  write_cmd
      ->add_option("-d,--description", description,
                   "description, [string/file]")
      ->default_val("");
  std::string meta_json_file = "";
  write_cmd->add_option("-m,--meta", meta_json_file, "JSON metadata file path")
      ->default_val("");
  std::string output_file;
  write_cmd->add_option("-o,--output", output_file, "output file path");

  uint16_t block_size = 8;
  write_cmd
      ->add_option("-b,--block-size", block_size,
                   "default block size 8, unit KB")
      ->default_val(8);
  uint8_t compress_level = 5;
  write_cmd
      ->add_option("-l,--compress-level", compress_level,
                   "default compress level 5")
      ->default_val(5);
  uint16_t zstd_dict_size = 100;
  write_cmd
      ->add_option("--zstd-dict-size", zstd_dict_size,
                   "default zstd dict size 100, unit KB")
      ->default_val(32);
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

  Logger::instance().set_level(spdlog::level::level_enum(log_level));

  if (version){
    std::cout<<"fstd version "<<FSTD_VERSION<<std::endl;
    return 0;
  }
  else if(*search_cmd) {
    if (ends_with(file_path, ".fstdd")) {
      fstd::FstddReader fstdd_reader(file_path);
      if (!fstdd_reader) {
        LOG_ERROR("file {} is not a valid fstdd file", file_path);
        return 1;
      }
      if (query_meta) {
        json meta = fstdd_reader.get_meta();
        std::cout << meta.dump(2) << std::endl;
      } else if (contains) {
        if (fstdd_reader.contains(word)) {
          std::cout << "yes" << std::endl;
          return 0;
        } else {
          std::cout << "no" << std::endl;
          return 1;
        }
      } else if (enumerate) {
        vector<string> all_keys;
        if (!fstdd_reader.extract_all_key(all_keys)) {
          return 1;
        } else {
          for (const auto &key : all_keys) {
            std::cout << key << "\n";
          }
          cout << all_keys.size() << endl;
        }
      } else {
        LOG_ERROR("fstdd does not support this search command.");
        return 1;
      }
    } else if (ends_with(file_path, ".fstdx")) {
      fstd::FstdxReader fstdx_reader(file_path);
      if (!fstdx_reader) {
        LOG_ERROR("file {} is not a valid fstdx file", file_path);
        return 1;
      }

      if (query_meta) {
        json meta = fstdx_reader.get_meta();
        std::cout << meta.dump(2) << std::endl;
      } else if (contains) {
        if (fstdx_reader.contains(word)) {
          std::cout << "yes" << std::endl;
          return 0;
        } else {
          std::cout << "no" << std::endl;
          return 1;
        }
      } else if (common_prefix) {
        auto result = fstdx_reader.common_prefix_search(word);
        if (result.empty()) {
          LOG_INFO("no match found");
          return 1;
        }
        for (const auto &p : result) {
          std::cout << *p << std::endl;
        }
      } else if (longest_common_prefix) {
        size_t len = fstdx_reader.longest_prefix_len(word);
        if (len == 0) {
          LOG_INFO("no match found");
          return 1;
        }
        std::cout << word.substr(0, len) << std::endl;
      } else if (predictive) {
        auto result = fstdx_reader.predictive_search(word);
        if (result.empty()) {
          LOG_INFO("no match found");
          return 1;
        }
        for (const auto &p : result) {
          std::cout << *p << std::endl;
        }
      } else if (edit_distance) {
        auto result = fstdx_reader.edit_distance_search(word, 1);
        if (result.empty()) {
          LOG_INFO("no match found");
          return 1;
        }
        for (const auto &p : result) {
          std::cout << *p << std::endl;
        }
      } else if (regex) {
        auto p_results = fstdx_reader.regex_search(word);
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
        std::vector<std::unique_ptr<pair<double, std::string>>> result =
            fstdx_reader.spellcheck_word(word);
        if (result.empty()) {
          LOG_INFO("no match found");
          return 1;
        }
        for (const auto &p : result) {
          std::cout << p->second << " -> " << p->first << "\n";
        }
        std::cout << std::flush;
      } else if (enumerate) {
        fstdx_reader.enumerate_print();
        return 0;
      } else {
        // exact match search
        std::vector<std::string> result;
        bool res = fstdx_reader.exact_match_search(word, result);
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
    }else{
      LOG_ERROR("[{}] is not a fstdx or fstdd to search",file_path);
      return 1;
    }
  } else if (*write_cmd) {
      std::cout << "input file path: " << write_input_file << "\n";
  std::cout << "index, value, step list: \n";
  std::cout << "output file path: " << output_file << "\n";
  std::cout << "default block size: " << block_size << " KB\n";
  std::cout << "default compress level: " << static_cast<int>(compress_level)
            << "\n";
  std::cout << "default zstd dict size: " << zstd_dict_size << " KB\n";
  std::cout << "default sorted false: " << (opt_sorted ? "true" : "false")
            << "\n";
  std::cout << "verbose: " << (verbose ? "true" : "false") << "\n";

    json meta_json;
    meta_json["Version"] = FSTD_VERSION;
    if (!meta_json_file.empty()) {
      std::string content;
      if (!read_file(meta_json_file, content)) {
        LOG_ERROR("Cannot open the file: {}", meta_json_file);
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

    if (fs::is_directory(write_input_file)) {
      if (output_file.empty()) { output_file = write_input_file + ".fstdd"; }
      fstd::FstddWriter fstdd_writer;
      int ret = fstdd_writer.compile_fstdd(
          write_input_file, output_file, meta_json, block_size, compress_level,
          write_worker_num, verbose);
      if (ret != 0) {
        LOG_ERROR("compile failed, return code: {}", ret);
        return ret;
      }
    } else if (fs::is_regular_file(write_input_file)) {
      if (output_file.empty()) {
        output_file = change_ext(write_input_file, ".fstdx");
      }
      fstd::FstdxWriter fstdx_writer;
      int ret = fstdx_writer.compile_fstdx(
          write_input_file, output_file, meta_json, block_size, compress_level,
          zstd_dict_size, write_worker_num, opt_sorted, verbose);
      if (ret != 0) {
        LOG_ERROR("compile failed, return code: {}", ret);
        return ret;
      }
    } else {
      LOG_ERROR("{} is not a valid directory or file.", write_input_file);
      return 1;
    }
    LOG_INFO("compile success");
    return 0;
  } else if (*extract_cmd) {
    if (ends_with(extract_input_file, ".fstdx")) {
      fstd::FstdxReader fstdx_reader(extract_input_file);
      if (!fstdx_reader) { return 1; }
      if (extract_output_path.empty()) {
        extract_output_path = change_ext(extract_input_file, "txt");
      } else {
        if (fs ::is_directory(extract_output_path)) {
          extract_input_file = (fs::path(extract_output_path) /
                                change_ext(extract_input_file, "txt"))
                                   .string();
        }
      }
      if (!fstdx_reader.extract(extract_output_path)) { return 1; }
    } else if (ends_with(extract_input_file, ".fstdd")) {
      fstd::FstddReader fstdd_reader(extract_input_file);
      if (!fstdd_reader) { return 1; }
      if (extract_output_path.empty()) {
        extract_output_path = "data";
      } else {
        if (fs::exists(extract_output_path) &&
            !fs::is_directory(extract_output_path)) {
          LOG_ERROR("{} is not a directory.", extract_output_path);
          return 1;
        }
      }
      if (key_file_path.empty()) {
        if (!fstdd_reader.extract_all(extract_output_path)) { return 1; }
      } else {
        if (!fstdd_reader.extract(key_file_path, extract_output_path)) {
          return 1;
        }
      }
    } else {
      LOG_ERROR("Only support fstdd/fstdx to extract");
      return 1;
    }
  } else {
    LOG_ERROR("Unknown Subcommand");
    return 1;
  }
  return 0;
}