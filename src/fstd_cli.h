#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <CLI/CLI.hpp>
#include <fstd/fstdd_reader.h>
#include <fstd/fstdd_writer.h>
#include <fstd/fstdx_reader.h>
#include <fstd/fstdx_searcher.h>
#include <fstd/fstdx_writer.h>

using namespace std;
using namespace fstd;
using json = nlohmann::json;
namespace fs = std::filesystem;

/**
 * @brief FSTD Command-Line Interface for dictionary engine operations
 * Supports search, write, extract for .fstdd and .fstdx files
 */
class FstdCli {
public:
  /**
   * @brief Constructor initializes CLI app and arguments
   */
  FstdCli(int argc, char **argv)
      : argc_(argc), argv_(argv),
        app_("fstd - A high-performance dictionary engine") {
    // ✅ FIXED: Enable full help for ALL subcommands in main -h
    app_.set_help_all_flag();
    app_.require_subcommand(); // Force user to select a subcommand
  }

  /**
   * @brief Main run entry: parse CLI and execute command
   * @return exit code (0 = success)
   */
  int run() {
    int ret = setup_cli();
    if (ret != 0) return ret;

    // Set global log level
    Logger::instance().set_level(
        static_cast<spdlog::level::level_enum>(log_level_));

    if (show_version_) {
      cout << "fstd version " << FSTD_VERSION << endl;
      return 0;
    }

    return execute_selected_command();
  }

private:
  //-------------------------------------------------------------------------
  // CLI Setup (Modular, Clean)
  //-------------------------------------------------------------------------
  void setup_common_options() {
    app_.footer("Powered by Finite State Transducer Technology");

    app_.add_flag("--verbose", verbose_, "Enable verbose debug logging");
    app_.add_flag("-v,--version", show_version_, "Show application version");
    app_.add_option("-l,--log-level", log_level_,
                    "Set log level [0=trace, 6=off]")
        ->default_val(4);
  }

  void setup_extract_subcommand() {
    extract_cmd_ =
        app_.add_subcommand("extract", "Extract data from .fstdx/.fstdd files");

    extract_cmd_
        ->add_option("input", extract_input_path_,
                     "Input .fstdx/.fstdd file path")
        ->required();

    extract_cmd_->add_option("-k,--key-file", key_file_path_,
                             "Path to file existig in .fstdd");
    extract_cmd_->add_option("-o,--output", extract_output_path_,
                             "Output directory or file path");
  }

  void setup_search_subcommand() {
    search_cmd_ =
        app_.add_subcommand("search", "Search keywords in .fstdx/.fstdd files");

    // Search modes
    search_cmd_->add_flag("-c,--contains", contains_,
                          "Check if key exists in dictionary");
    search_cmd_->add_flag("-p,--predictive", predictive_,
                          "Perform predictive search");
    search_cmd_->add_flag("-r,--regex", regex_search_,
                          "Run regex pattern search");
    search_cmd_->add_flag("-s,--spellcheck", spellcheck_, "Spell-check a word");
    search_cmd_->add_flag("-g,--suggest", suggest_, "Get word suggestions");
    search_cmd_->add_flag("--common-prefix", common_prefix_,
                          "Search common prefix matches");
    search_cmd_->add_flag("-l,--longest-prefix", longest_prefix_,
                          "Find longest common prefix");
    search_cmd_->add_flag("-u,--enumerate", enumerate_,
                          "List all keys in the dictionary");
    search_cmd_->add_flag("-m,--meta", show_meta_,
                          "Display file metadata (JSON)");

    // Parameters
    search_cmd_
        ->add_option("-e,--edit-distance", edit_distance_,
                     "Max edit distance for fuzzy search")
        ->default_val(0);
    search_cmd_
        ->add_option("-P,--prefix-distance", prefix_distance_,
                     "Max distance for prefix distance search")
        ->default_val(0);
    search_cmd_
        ->add_option("-w,--worker-threads", search_workers_,
                     "Number of search worker threads")
        ->default_val(0);
    search_cmd_
        ->add_option("-f,--dictionary", dict_files_,
                     "Add multiple .fstdx files for batch search")
        ->delimiter(',');

    // ✅ FIXED: Make positional args optional when using -f (batch search)
    search_cmd_->add_option("key", search_key_, "Key or pattern to search")
        ->required(false);
    search_cmd_
        ->add_option("file", file_path_, "Single input dictionary file path")
        ->required(false);
  }

  void setup_write_subcommand() {
    write_cmd_ =
        app_.add_subcommand("write", "Compile text/directory to .fstdx/.fstdd");

    write_cmd_
        ->add_option("-f,--input", write_input_path_,
                     "Input text file or directory path")
        ->required();

    write_cmd_
        ->add_option("-t,--title", dict_title_,
                     "Dictionary title (string or file path)")
        ->default_val("");
    write_cmd_
        ->add_option("-d,--desc", dict_description_,
                     "Dictionary description (string or file path)")
        ->default_val("");
    write_cmd_
        ->add_option("-m,--meta", meta_file_path_, "JSON metadata file path")
        ->default_val("");
    write_cmd_->add_option("-o,--output", output_path_,
                           "Output .fstdx/.fstdd file path");

    // Advanced options
    write_cmd_
        ->add_option("-b,--block-size", block_size_kb_, "Block size in KB")
        ->default_val(8);
    write_cmd_
        ->add_option("-c,--compress-level", compress_level_,
                     "Zstd compression level [0-22]")
        ->default_val(5);
    write_cmd_
        ->add_option("-z,--zstd-dict-size", zstd_dict_kb_,
                     "Zstd dictionary size in KB")
        ->default_val(100);
    write_cmd_
        ->add_flag("-s,--sorted", pre_sorted_,
                   "Set to true if input is already sorted")
        ->default_val(false);
    write_cmd_
        ->add_option("-w,--worker-threads", write_workers_,
                     "Number of compression worker threads")
        ->default_val(0);
  }

  /**
   * @brief Initialize all CLI structures and parse input
   */
  int setup_cli() {
    setup_common_options();
    setup_extract_subcommand();
    setup_search_subcommand();
    setup_write_subcommand();

    try {
      CLI11_PARSE(app_, argc_, argv_);
      if (file_path_.empty()) { file_path_ = search_key_; }
    } catch (const CLI::ParseError &e) { return app_.exit(e); }
    return 0;
  }

  //-------------------------------------------------------------------------
  // Command Execution
  //-------------------------------------------------------------------------
  int execute_selected_command() {
    if (*search_cmd_) return run_search();
    if (*write_cmd_) return run_write();
    if (*extract_cmd_) return run_extract();

    LOG_ERROR("No valid subcommand selected. Use -h for help.");
    return 1;
  }

  //-------------------------------------------------------------------------
  // Search Implementations (Cleaned Logic)
  //-------------------------------------------------------------------------
  int run_fstdd_search() {
    FstddReader reader(file_path_);
    if (!reader) {
      LOG_ERROR("Invalid or corrupted .fstdd file: {}", file_path_);
      return 1;
    }

    if (show_meta_) {
      cout << reader.get_meta().dump(2) << endl;
      return 0;
    }
    if (contains_) {
      cout << (reader.contains(search_key_) ? "yes" : "no") << endl;
      return reader.contains(search_key_) ? 0 : 1;
    }
    if (enumerate_) {
      vector<string> keys;
      if (!reader.extract_all_key(keys)) { return 1; }
      for (const auto &k : keys) {
        cout << k << "\n";
      }
      cout << "Total files: " << keys.size() << endl;
      return 0;
    }

    LOG_ERROR("Unsupported operation for .fstdd files");
    return 1;
  }

  int run_fstdx_search() {
    FstdxReader reader(file_path_);
    if (!reader) {
      LOG_ERROR("Invalid or corrupted .fstdx file: {}", file_path_);
      return 1;
    }

    if (show_meta_) {
      cout << reader.get_meta().dump(2) << endl;
      return 0;
    }
    if (contains_) {

      if (reader.contains(search_key_)) {
        cout << "yes" << endl;
        return 0;
      } else {
        cout << "no" << endl;
        return 1;
      }
    }
    if (common_prefix_) {
      auto result = reader.common_prefix_search(search_key_);
      if (result.empty()) {
        LOG_INFO("no match found");
        return 1;
      }
      for (const auto &p : result) {
        cout << *p << endl;
      }
      return 0;
    }
    if (longest_prefix_) {
      size_t len = reader.longest_prefix_len(search_key_);
      if (len == 0) {
        LOG_INFO("no match found");
        return 1;
      }
      cout << search_key_.substr(0, len) << endl;
      return 0;
    }
    if (predictive_) {
      auto result = reader.predictive_search(search_key_);
      if (result.empty()) {
        LOG_INFO("no match found");
        return 1;
      }
      for (const auto &p : result) {
        cout << *p << endl;
      }
      return 0;
    }
    if (edit_distance_ != 0) {
      auto result = reader.edit_distance_search(search_key_, edit_distance_);
      if (result.empty()) {
        LOG_INFO("no match found");
        return 1;
      }
      for (const auto &p : result) {
        cout << *p << endl;
      }
      return 0;
    }
    if (regex_search_) {
      auto p_results = reader.regex_search(search_key_);
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
        cout << *p << endl;
      }
      return 0;
    }
    if (suggest_) {
      vector<unique_ptr<pair<double, string>>> result =
          reader.suggest(search_key_);
      if (result.empty()) {
        LOG_INFO("no match found");
        return 1;
      }
      sort(result.begin(), result.end(),
           [&](const auto &x, const auto &y) { return x->first > y->first; });
      for (const auto &p : result) {
        cout << p->second << " -> " << p->first << "\n";
      }
      cout << flush;
      return 0;
    }
    if (spellcheck_) {
      vector<unique_ptr<pair<double, string>>> result =
          reader.spellcheck_word(search_key_);
      if (result.empty()) {
        LOG_INFO("no match found");
        return 1;
      }
      for (const auto &p : result) {
        cout << p->second << " -> " << p->first << "\n";
      }
      cout << flush;
      return 0;
    }
    if (enumerate_) {
      reader.enumerate_print();
      return 0;
    }

    {
      // default to exact match search
      vector<string> result;
      bool res = reader.exact_match_search(search_key_, result);
      if (!res) {
        LOG_INFO("no match found");
        return 1;
      }
      for (const string &s : result) {
        cout << "------------------------------" << endl;
        cout << s << endl;
        cout << "------------------------------" << endl;
      }
      return 0;
    }
    return 0;
  }

  int run_batch_search() {
    FstdxSearcher searcher(search_workers_);
    for (const auto &f : dict_files_) {
      if (!searcher.insert(f, f)) { return 1; }
    }
    if (contains_) {
      cout << "search_key_: " << search_key_ << endl;
      if (searcher.contains(search_key_, dict_files_)) {
        cout << "yes" << endl;
        return 0;
      } else {
        cout << "no" << endl;
        return 1;
      }
    }
    if (common_prefix_) {
      auto result = searcher.common_prefix_search(search_key_, dict_files_);
      if (result.empty()) {
        LOG_INFO("no match found");
        return 1;
      }
      for (const auto &p : result) {
        cout << p << endl;
      }
      return 0;
    }
    if (longest_prefix_) {
      size_t len =
          searcher.longest_prefix_len(search_key_, dict_files_);
      if (len == 0) {
        LOG_INFO("no match found");
        return 1;
      }
      cout << search_key_.substr(0, len) << endl;
      return 0;
    }
    if (predictive_) {
      auto result = searcher.predictive_search(search_key_, dict_files_);
      if (result.empty()) {
        LOG_INFO("no match found");
        return 1;
      }
      for (const auto &p : result) {
        cout << p << endl;
      }
      return 0;
    }
    if (prefix_distance_ != 0) {
      vector<string> prior_sufs{"する", "う", "く", "ぐ", "す",   "つ", "ぬ",
                                "ぶ",   "む", "る", "い", "하다", "다"};
      searcher.insert_prior_suffix(prior_sufs);
      auto result = searcher.prefix_distance_search(search_key_, dict_files_,
                                                    prefix_distance_);
      if (result.empty()) {
        LOG_INFO("no match found");
        return 1;
      }
      for (const auto &p : result) {
        cout << p << endl;
      }
      return 0;
    }
    if (edit_distance_ != 0) {
      auto result = searcher.edit_distance_search(search_key_, dict_files_,
                                                  edit_distance_);
      if (result.empty()) {
        LOG_INFO("no match found");
        return 1;
      }
      for (const auto &p : result) {
        cout << p << endl;
      }
      return 0;
    }
    if (regex_search_) {
      auto p_results = searcher.regex_search(search_key_, dict_files_);
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
        cout << p << endl;
      }
      return 0;
    }
    if (suggest_) {
      vector<string> result = searcher.suggest(search_key_, dict_files_);
      if (result.empty()) {
        LOG_INFO("no match found");
        return 1;
      }
      for (const auto &s : result) {
        cout << s << "\n";
      }
      cout << flush;
      return 0;
    }

    {
      // default to exact match search
      unordered_map<string, vector<string>> results =
          searcher.exact_match_search(search_key_, dict_files_);
      bool has_matched = false;
      for (const auto &p : results) {
        cout << "# " << p.first << ":\n";
        if (!p.second.empty()) { has_matched = true; }
        for (const string &s : p.second) {
          cout << "------------------------------" << endl;
          cout << s << endl;
          cout << "------------------------------" << endl;
        }
      }
      if (!has_matched) {
        LOG_INFO("no match found");
        return 1;
      }
      return 0;
    }
    return 0;
  }

  int run_search() {
    if (!dict_files_.empty()) return run_batch_search();
    if (file_path_.ends_with(".fstdd")) return run_fstdd_search();
    if (file_path_.ends_with(".fstdx")) return run_fstdx_search();

    LOG_ERROR("Unsupported file type for search: {}", file_path_);
    return 1;
  }

  //-------------------------------------------------------------------------
  // Write Command (Cleaned + Fixed Duplicates)
  //-------------------------------------------------------------------------
  void print_write_config() {
    cout << "\n=== Compile Configuration ===" << endl;
    cout << "Input path:     " << write_input_path_ << endl;
    cout << "Output path:    " << output_path_ << endl;
    cout << "Block size:     " << block_size_kb_ << " KB" << endl;
    cout << "Compression:    " << static_cast<int>(compress_level_) << endl;
    cout << "Zstd dict size: " << zstd_dict_kb_ << " KB" << endl;
    cout << "Pre-sorted:     " << boolalpha << pre_sorted_ << endl;
    cout << "============================\n" << endl;
  }

  int run_write() {
    json meta = {{"Version", FSTD_VERSION}};
    if (!meta_file_path_.empty()) {
      string content;
      if (!read_file(meta_file_path_, content)) {
        LOG_ERROR("Cannot open the file: {}", meta_file_path_);
        return 1;
      } else {
        try {
          meta = json::parse(content);
        } catch (const json::exception &e) {
          LOG_ERROR("file {} format error: {}", meta_file_path_, e.what());
          return 1;
        } catch (const exception &e) {
          LOG_ERROR("file {} read error: {}", meta_file_path_, e.what());
          return 1;
        }
      }
    }
    if (!dict_title_.empty()) {
      string content;
      meta["Title"] = read_file(dict_title_, content) ? content : dict_title_;
    }
    if (!dict_description_.empty()) {
      string content;
      meta["Description"] =
          read_file(dict_description_, content) ? content : dict_description_;
    }

    if (fs::is_directory(write_input_path_)) {
      if (output_path_.empty()) output_path_ = write_input_path_ + ".fstdd";
      print_write_config();
      FstddWriter writer;
      int ret = writer.compile_fstdd(write_input_path_, output_path_, meta,
                                     block_size_kb_, compress_level_,
                                     write_workers_, verbose_);
      if (ret != 0) {
        LOG_ERROR("Compilation failed");
        return ret;
      }
    } else if (fs::is_regular_file(write_input_path_)) {
      if (output_path_.empty())
        output_path_ =
            fs::path(write_input_path_).replace_extension("fstdx").string();
      print_write_config();
      FstdxWriter writer;
      int ret =
          writer.compile_fstdx(write_input_path_, output_path_, meta,
                               block_size_kb_, compress_level_, zstd_dict_kb_,
                               write_workers_, pre_sorted_, verbose_);
      if (ret != 0) {
        LOG_ERROR("Compilation failed");
        return ret;
      }
    } else {
      LOG_ERROR("Input path is not a valid file or directory");
      return 1;
    }

    LOG_INFO("Compilation completed successfully");
    return 0;
  }

  //-------------------------------------------------------------------------
  // Extract Command
  //-------------------------------------------------------------------------
  int run_extract() {
    const string &path = extract_input_path_;
    if (path.ends_with(".fstdx")) {
      FstdxReader reader(path);
      if (!reader) { return 1; }
      string out = extract_output_path_.empty()
                       ? fs::path(path).replace_extension("txt").string()
                       : extract_output_path_;
      return reader.extract(out) ? 0 : 1;
    }
    if (path.ends_with(".fstdd")) {
      FstddReader reader(path);
      if (!reader) return 1;
      string out = extract_output_path_.empty() ? "data" : extract_output_path_;
      return key_file_path_.empty() ? reader.extract_all(out) ? 0 : 1
             : reader.extract(key_file_path_, out) ? 0
                                                   : 1;
    }

    LOG_ERROR("Extract only supports .fstdx and .fstdd files");
    return 1;
  }

private:
  // CLI Core
  int argc_;
  char **argv_;
  CLI::App app_;

  // Common Options
  bool verbose_ = false;
  bool show_version_ = false;
  uint8_t log_level_ = 4;

  // Extract Subcommand
  CLI::App *extract_cmd_ = nullptr;
  string extract_input_path_;
  string key_file_path_;
  string extract_output_path_;

  // Search Subcommand
  CLI::App *search_cmd_ = nullptr;
  bool contains_ = false;
  bool predictive_ = false;
  size_t prefix_distance_ = 0;
  size_t edit_distance_ = 0;
  bool regex_search_ = false;
  bool spellcheck_ = false;
  bool suggest_ = false;
  bool common_prefix_ = false;
  bool longest_prefix_ = false;
  bool enumerate_ = false;
  bool show_meta_ = false;
  size_t search_workers_ = 0;
  string file_path_;
  string search_key_;
  vector<string> dict_files_;

  // Write Subcommand
  CLI::App *write_cmd_ = nullptr;
  string write_input_path_;
  string dict_title_;
  string dict_description_;
  string meta_file_path_;
  string output_path_;
  uint16_t block_size_kb_ = 8;
  uint8_t compress_level_ = 5;
  uint16_t zstd_dict_kb_ = 100;
  bool pre_sorted_ = false;
  size_t write_workers_ = 0;
};
