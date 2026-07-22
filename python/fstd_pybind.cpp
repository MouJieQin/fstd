#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>

#include "fstd/fstdd_reader.h"
#include "fstd/fstdd_writer.h"
#include "fstd/fstdx_reader.h"
#include "fstd/fstdx_searcher.h"
#include "fstd/fstdx_writer.h"

namespace py = pybind11;

std::vector<std::string>
convert(const std::vector<std::unique_ptr<std::string>> &res) {
  std::vector<std::string> result;
  result.reserve(res.size());
  for (auto &ptr : res) {
    result.emplace_back(std::move(*ptr));
  }
  return result;
}

std::vector<std::pair<double, std::string>> convert(
    const std::vector<std::unique_ptr<std::pair<double, std::string>>> &res) {
  std::vector<std::pair<double, std::string>> result;
  result.reserve(res.size());
  for (auto &ptr : res) {
    result.emplace_back(*ptr);
  }
  return result;
}

std::pair<std::vector<std::string>, std::string>
convert(const std::pair<std::vector<std::unique_ptr<std::string>>, std::string>
            &res) {
  std::pair<std::vector<std::string>, std::string> result;
  result.first.reserve(res.first.size());
  for (auto &ptr : res.first) {
    result.first.emplace_back(std::move(*ptr));
  }
  result.second = res.second;
  return result;
}

PYBIND11_MODULE(_native, m) {
  m.doc() = "Python binding for fstd dictionary engine";

  m.def(
      "get_version", []() { return FSTD_VERSION; },
      "Get fstd library version.");

  m.def(
      "set_log_level",
      [](uint8_t log_level) {
        Logger::instance().set_level(
            static_cast<spdlog::level::level_enum>(log_level));
      },
      R"(Set the log level for fstd library.

Args:
    log_level: Log level value, range 0-6. 0=trace, 1=debug, 2=info,
        3=warn, 4=error, 5=critical, 6=off.
)");

  py::class_<fstd::FstddReader>(m, "FstddReader")
      .def(py::init<const std::string &>(), py::arg("fstdd_file"),
           R"(Initialize the reader with an fstdd file.

Args:
    fstdd_file: Path to the fstdd file.
)")
      .def(
          "__bool__",
          [](const fstd::FstddReader &self) -> bool {
            return static_cast<bool>(self);
          },
          "Check if the fstdd reader is valid.")
      .def("is_valid", &fstd::FstddReader::operator bool,
           R"(Check if the fstdd reader is valid.

Returns:
    bool: True if the reader is valid, False otherwise.
)")
      .def(
          "get_meta",
          [](fstd::FstddReader &self) { return self.get_meta().dump(); },
          R"(Get the meta of the fstdd file.

Returns:
    str: Meta info as a JSON string.
)")
      .def(
          "get_header",
          [](fstd::FstddReader &self) { return self.get_header().dump(); },
          R"(Get the header of the fstdd file.

Returns:
    str: Header info as a JSON string.
)")
      .def("contains", &fstd::FstddReader::contains, py::arg("key_path"),
           R"(Check whether a key_path exists in the fstdd file.

Args:
    key_path: The key path to check.

Returns:
    bool: True if the key exists, False otherwise.
)")
      .def(
          "extract_all_key",
          [](fstd::FstddReader &self) {
            std::vector<std::string> all_keys;
            self.extract_all_key(all_keys);
            return all_keys;
          },
          R"(Extract all keys from the fstdd file.

Returns:
    list[str]: All keys stored in the fstdd archive.
)")
      .def("extract", &fstd::FstddReader::extract, py::arg("key"),
           py::arg("dst_dir") = "data",
           R"(Extract a single file by key to the destination directory.

Args:
    key: The key of the file to extract.
    dst_dir: Destination directory path. Defaults to ``"data"``.

Returns:
    bool: True if extraction succeeds, False otherwise.
)")
      .def("extract_all", &fstd::FstddReader::extract_all,
           py::arg("dst_dir") = "data",
           R"(Extract all files in the fstdd file to dst_dir.

Args:
    dst_dir: Destination directory path.

Returns:
    bool: True if extraction succeeds, False otherwise.
)");

  py::class_<fstd::FstddWriter>(m, "FstddWriter")
      .def(py::init<>())
      .def(
          "compile_fstdd",
          [](fstd::FstddWriter &self, const std::string &data_path,
             const std::string &output_file, const std::string &meta_json_str,
             size_t block_size_kb, size_t compress_level, size_t worker_num,
             bool opt_verbose) {
            return self.compile_fstdd(data_path, output_file, meta_json_str,
                                      block_size_kb, compress_level, worker_num,
                                      opt_verbose);
          },
          py::arg("data_path"), py::arg("output_file"),
          py::arg("meta_json_str"), py::arg("block_size_kb"),
          py::arg("compress_level"), py::arg("worker_num"),
          py::arg("opt_verbose"),
          R"(Compile an fstdd file from a data file or directory.

Args:
    data_path: Path to the source data file or directory.
    output_file: Path to the output fstdd file.
    meta_json_str: Meta information as a JSON string.
    block_size_kb: Block size in KB.
    compress_level: Compression level, range [0, 22].
    worker_num: Number of worker threads used for compilation.
    opt_verbose: Whether to print verbose progress info.

Returns:
    int: 0 on success, non-zero error code otherwise.
)")
      .def(
          "push_file_stream",
          [](fstd::FstddWriter &self, const std::string &file_path,
             py::bytes data) {
            std::string_view buf(data);
            return self.push_file_stream(file_path, buf);
          },
          py::arg("file_path"), py::arg("data"),
          R"(Push a file stream into the writer.

Args:
    file_path: Key path of the file to push.
    data: Raw bytes content of the file.

Returns:
    bool: True if the push succeeds, False otherwise.
)")
      .def(
          "compile_fstdd",
          [](fstd::FstddWriter &self, size_t file_stream_num,
             const std::string &output_file, const std::string &meta_json_str,
             size_t block_size_kb, size_t compress_level, size_t worker_num,
             bool opt_verbose) {
            py::gil_scoped_release
                release; // Release the GIL to allow other Python
                         // threads to run while compiling
            std::vector<std::string> data_paths;
            return self.compile_fstdd(data_paths, output_file, meta_json_str,
                                      block_size_kb, compress_level, worker_num,
                                      opt_verbose, file_stream_num);
          },
          py::arg("file_stream_num"), py::arg("output_file"),
          py::arg("meta_json_str"), py::arg("block_size_kb"),
          py::arg("compress_level"), py::arg("worker_num"),
          py::arg("opt_verbose"),
          R"(Compile an fstdd file from previously pushed file streams.

Args:
    file_stream_num: Number of pushed file streams to compile.
    output_file: Path to the output fstdd file.
    meta_json_str: Meta information as a JSON string.
    block_size_kb: Block size in KB.
    compress_level: Compression level, range [0, 22].
    worker_num: Number of worker threads used for compilation.
    opt_verbose: Whether to print verbose progress info.

Returns:
    bool: True if compilation succeeds, False otherwise.
)");

  py::class_<fstd::FstdxReader>(m, "FstdxReader")
      .def(py::init<const std::string &>(), py::arg("fstdx_path"),
           R"(Initialize the reader with an fstdx dictionary file.

Args:
    fstdx_path: Path to the fstdx file.
)")
      .def("__bool__", &fstd::FstdxReader::operator bool,
           R"(Check if the fstdx reader is valid.

Returns:
    bool: True if the reader is valid, False otherwise.
)")
      .def("is_valid", &fstd::FstdxReader::operator bool,
           R"(Check if the fstdx file is valid.

Returns:
    bool: True if the file is valid, False otherwise.
)")
      .def(
          "get_meta",
          [](fstd::FstdxReader &self) { return self.get_meta().dump(); },
          R"(Get the meta of the fstdx file.

Returns:
    str: Meta info as a JSON string.
)")
      .def(
          "get_header",
          [](fstd::FstdxReader &self) { return self.get_header().dump(); },
          R"(Get the header of the fstdx file.

Returns:
    str: Header info as a JSON string.
)")
      .def("get_key_size", &fstd::FstdxReader::get_key_size,
           R"(Get the total key count of entry words.

Returns:
    int: Number of entry word keys.
)")
      .def("get_fst_key_size", &fstd::FstdxReader::get_fst_key_size,
           R"(Get the key count of the FST index.

The FST key count is less than or equal to the entry key size due to
duplicate entries sharing the same key.

Returns:
    int: Number of unique keys in the FST index.
)")
      .def(
          "extract_values",
          [](fstd::FstdxReader &self) { return self.extract_values(); },
          R"(Extract all values of the dictionary.

Returns:
    list[str]: All entry values in the dictionary.
)")
      .def("contains", &fstd::FstdxReader::contains, py::arg("word"),
           R"(Check whether a word exists in the dictionary.

Args:
    word: The word to check.

Returns:
    bool: True if the word exists, False otherwise.
)")
      .def(
          "exact_match_search",
          [](fstd::FstdxReader &self, const std::string &word) {
            std::vector<std::string> result;
            self.exact_match_search(word, result);
            return result;
          },
          py::arg("word"),
          R"(Perform an exact match search for the given word.

Args:
    word: The word to search.

Returns:
    list[str]: Matching entry values; empty if the word is not found.
)")
      .def(
          "common_prefix_search",
          [](fstd::FstdxReader &self, const std::string &word) {
            return convert(self.common_prefix_search(word));
          },
          py::arg("word"),
          R"(Perform a common prefix search for the given word.

Args:
    word: The word whose prefixes are searched.

Returns:
    list[str]: Words in the dictionary that are prefixes of the input word.
)")
      .def("longest_prefix_len", &fstd::FstdxReader::longest_prefix_len,
           py::arg("word"),
           R"(Get the length of the longest matching prefix in the dictionary.

Args:
    word: The word to search.

Returns:
    int: Length of the longest common prefix found.
)")
      .def(
          "predictive_search",
          [](fstd::FstdxReader &self, const std::string &word) {
            return convert(self.predictive_search(word));
          },
          py::arg("word"),
          R"(Perform a predictive (prefix) search.

Returns all words in the dictionary that start with the given prefix.

Args:
    word: The prefix to search.

Returns:
    list[str]: Words starting with the given prefix.
)")
      .def(
          "edit_distance_search",
          [](fstd::FstdxReader &self, const std::string &word,
             size_t distance) {
            return convert(self.edit_distance_search(word, distance));
          },
          py::arg("word"), py::arg("distance") = 1,
          R"(Perform an edit distance (fuzzy) search.

Finds all words within the given Levenshtein distance from the input.

Args:
    word: The word to search.
    distance: Maximum allowed edit distance. Defaults to 1.

Returns:
    list[str]: Words whose edit distance is less than or equal to ``distance``.
)")
      .def(
          "suggest",
          [](fstd::FstdxReader &self, const std::string &word) {
            return convert(self.suggest(word));
          },
          py::arg("word"),
          R"(Get spelling suggestions for the given word.

Args:
    word: The word to get suggestions for.

Returns:
    list[tuple[float, str]]: Suggested words paired with their similarity
        scores, sorted by similarity.
)")
      .def(
          "regex_search",
          [](fstd::FstdxReader &self, const std::string &pattern,
             size_t thread) {
            if (thread == 1) {
              return convert(self.regex_search(pattern));
            } else {
              ThreadPool pool(thread);
              return convert(self.regex_search(pattern, pool));
            }
          },
          py::arg("pattern"), py::arg("thread") = 1,
          R"(Perform a regular expression search on the dictionary.

Args:
    pattern: The regex pattern to match.
    thread: Number of threads to use. Defaults to 1.

Returns:
    tuple[list[str], str]: Matched words at index 0, error message (if any)
        at index 1.
)")
      .def(
          "spellcheck_word",
          [](fstd::FstdxReader &self, const std::string &word, size_t limit) {
            return convert(self.spellcheck_word(word, limit));
          },
          py::arg("word"), py::arg("limit") = 10,
          R"(Spell-check a word and return the best suggestions.

Args:
    word: The word to spell-check.
    limit: Maximum number of suggestions to return. Defaults to 10.

Returns:
    list[tuple[float, str]]: Suggested words with similarity scores.
)")
      .def("enumerate_print", &fstd::FstdxReader::enumerate_print,
           R"(Print the entire dictionary to the console.

Returns:
    None.
)")
      .def("extract", &fstd::FstdxReader::extract, py::arg("output_file"),
           R"(Extract the raw text of the dictionary to a file.

Args:
    output_file: Path to the output text file.

Returns:
    bool: True if extraction succeeds, False otherwise.
)")
      .def(
          "extract_keys",
          [](fstd::FstdxReader &self) -> std::vector<std::string> {
            return self.extract_keys();
          },
          R"(Extract all keys (headwords) of the dictionary.

Returns:
    list[str]: All keys in the dictionary.
)");

  py::class_<fstd::FstdxWriter>(m, "FstdxWriter")
      .def(py::init<>())
      .def(
          "compile_fstdx",
          [](fstd::FstdxWriter &self, const std::string &input_file,
             const std::string &output_file, const std::string &meta_json_str,
             uint16_t block_size_kb, uint8_t compress_level,
             uint16_t zstd_dict_size_kb, size_t worker_num, bool opt_sorted,
             bool opt_verbose) {
            return self.compile_fstdx(input_file, output_file, meta_json_str,
                                      block_size_kb, compress_level,
                                      zstd_dict_size_kb, worker_num, opt_sorted,
                                      opt_verbose);
          },
          py::arg("input_file"), py::arg("output_file"),
          py::arg("meta_json_str"), py::arg("block_size_kb"),
          py::arg("compress_level"), py::arg("zstd_dict_size_kb"),
          py::arg("worker_num"), py::arg("opt_sorted"), py::arg("opt_verbose"),
          R"(Compile an fstdx file from a plain text input file.

Args:
    input_file: Path to the source dictionary text file.
    output_file: Path to the output fstdx file.
    meta_json_str: Meta information as a JSON string.
    block_size_kb: Block size in KB.
    compress_level: Zstd compression level, range [0, 22].
    zstd_dict_size_kb: Zstd dictionary size in KB.
    worker_num: Number of worker threads used for compilation.
    opt_sorted: Whether to sort values by key.
    opt_verbose: Whether to print verbose progress info.

Returns:
    bool: True if compilation succeeds, False otherwise.
)")
      .def(
          "compile_fstdx",
          [](fstd::FstdxWriter &self, const std::string &output_file,
             py::list py_keys, py::list py_values,
             const std::string &meta_json_str, uint16_t block_size_kb,
             uint8_t compress_level, uint16_t zstd_dict_size_kb,
             size_t worker_num, bool opt_sorted, bool opt_verbose) {
            std::vector<std::string> keys =
                py_keys.cast<std::vector<std::string>>();
            std::vector<std::string> values =
                py_values.cast<std::vector<std::string>>();

            return self.compile_fstdx(
                output_file, std::move(keys), std::move(values), meta_json_str,
                block_size_kb, compress_level, zstd_dict_size_kb, worker_num,
                opt_sorted, opt_verbose);
          },
          py::arg("output_file"), py::arg("keys"), py::arg("values"),
          py::arg("meta_json_str"), py::arg("block_size_kb"),
          py::arg("compress_level"), py::arg("zstd_dict_size_kb"),
          py::arg("worker_num"), py::arg("opt_sorted"), py::arg("opt_verbose"),
          R"(Compile an fstdx file from in-memory key and value lists.

Args:
    output_file: Path to the output fstdx file.
    keys: List of headword keys.
    values: List of entry values, parallel to ``keys``.
    meta_json_str: Meta information as a JSON string.
    block_size_kb: Block size in KB.
    compress_level: Zstd compression level, range [0, 22].
    zstd_dict_size_kb: Zstd dictionary size in KB.
    worker_num: Number of worker threads used for compilation.
    opt_sorted: Whether to sort values by key.
    opt_verbose: Whether to print verbose progress info.

Returns:
    bool: True if compilation succeeds, False otherwise.
)");

  py::class_<fstd::FstdxSearcher>(m, "FstdxSearcher")
      .def(py::init<size_t>(), py::arg("worker_num") = 0,
           R"(Initialize the searcher with a worker thread count.

Args:
    worker_num: Number of threads for parallel search. Defaults to 0,
        which auto-detects available CPU threads.
)")
      .def(py::init<const std::string &, size_t>(), py::arg("meta_json_path"),
           py::arg("worker_num") = 0,
           R"(Initialize the searcher from a meta JSON file.

Args:
    meta_json_path: Path to the meta JSON file describing loaded dictionaries.
    worker_num: Number of threads for parallel search. Defaults to 0,
        which auto-detects available CPU threads.
)")
      .def(
          "__bool__",
          [](fstd::FstdxSearcher &self) { return self.operator bool(); },
          R"(Check if the searcher is valid.

Returns:
    bool: True if the searcher is valid, False otherwise.
)")

      .def("is_valid", &fstd::FstdxSearcher::operator bool,
           R"(Check if the searcher is valid.

Returns:
    bool: True if the searcher is valid, False otherwise.
)")

      .def(
          "extract_if_exists", &fstd::FstdxSearcher::extract_if_exists,
          py::arg("name"), py::arg("file_path"), py::arg("dst_dir"),
          R"(Extract a file if exists from the fstdd archive paired with an fstdx dictionary.

The fstdd file is expected to reside in the same directory as the fstdx file.

Args:
    name: Name of the dictionary.
    file_path: Key path of the file to extract inside the fstdd archive.
    dst_dir: Destination directory.

Returns:
    bool: True if extraction succeeds, False otherwise.
)")

      .def(
          "extract",
          [](fstd::FstdxSearcher &self, const std::string &name,
             const std::string &file_path, const std::string &dst_dir) {
            if (dst_dir.empty()) { return self.extract(name, file_path); }
            return self.extract(name, file_path, dst_dir);
          },
          py::arg("name"), py::arg("file_path"), py::arg("dst_dir") = "",
          R"(Extract a file from the fstdd archive paired with an fstdx dictionary.

The fstdd file is expected to reside in the same directory as the fstdx file.

Args:
    name: Name of the dictionary.
    file_path: Key path of the file to extract inside the fstdd archive.
    dst_dir: Destination directory. If empty, uses the default directory.

Returns:
    bool: True if extraction succeeds, False otherwise.
)")

      .def(
          "contains_file", &fstd::FstdxSearcher::contains, py::arg("key_path"),
          py::arg("name"),
          R"(Check whether a file exists in the fstdd archives paired with an fstdx dictionary.

Args:
    key_path: The path as a key to check.
    name: Name of the dictionary.

Returns:
    bool: True if the file is found in any of the fstdd archives.
)")

      .def("contains", &fstd::FstdxSearcher::contains, py::arg("word"),
           py::arg("names"),
           R"(Check whether a word exists in the specified dictionaries.

Args:
    word: The word to check.
    names: List of dictionary names to search.

Returns:
    bool: True if the word is found in any of the dictionaries.
)")
      .def(
          "exact_match_search",
          [](fstd::FstdxSearcher &self, const std::string &word,
             const std::string &name) {
            return self.exact_match_search(word, name);
          },
          py::arg("word"), py::arg("name"),
          R"(Perform an exact match search on a single dictionary.

Args:
    word: The word to search.
    name: Name of the dictionary to search.

Returns:
    list[str]: Matching entry values.
)")
      .def(
          "exact_match_search",
          [](fstd::FstdxSearcher &self, const std::string &word,
             const std::vector<std::string> &names) {
            return self.exact_match_search(word, names);
          },
          py::arg("word"), py::arg("names"),
          R"(Perform an exact match search across multiple dictionaries.

Args:
    word: The word to search.
    names: List of dictionary names to search.

Returns:
    list[str]: Matching entry values from all specified dictionaries.
)")
      .def("common_prefix_search", &fstd::FstdxSearcher::common_prefix_search,
           py::arg("word"), py::arg("names"),
           R"(Perform a common prefix search across multiple dictionaries.

Args:
    word: The word whose prefixes are searched.
    names: List of dictionary names to search.

Returns:
    list[str]: Words that are prefixes of the input word.
)")
      .def("longest_prefix_len", &fstd::FstdxSearcher::longest_prefix_len,
           py::arg("word"), py::arg("names"),
           R"(Get the length of the longest matching prefix across dictionaries.

Args:
    word: The word to search.
    names: List of dictionary names to search.

Returns:
    int: Length of the longest common prefix found.
)")
      .def("edit_distance_search", &fstd::FstdxSearcher::edit_distance_search,
           py::arg("word"), py::arg("names"), py::arg("edit_distance") = 1,
           R"(Perform an edit distance search across multiple dictionaries.

Args:
    word: The word to search.
    names: List of dictionary names to search.
    edit_distance: Maximum allowed edit distance. Defaults to 1.

Returns:
    list[str]: Matching words within the edit distance threshold.
)")
      .def("predictive_search", &fstd::FstdxSearcher::predictive_search,
           py::arg("word"), py::arg("names"),
           R"(Perform a predictive (prefix) search across multiple dictionaries.

Args:
    word: The prefix to search.
    names: List of dictionary names to search.

Returns:
    list[str]: Words starting with the given prefix.
)")
      .def("suggest", &fstd::FstdxSearcher::suggest, py::arg("word"),
           py::arg("names"),
           R"(Get spelling suggestions across multiple dictionaries.

Args:
    word: The word to get suggestions for.
    names: List of dictionary names to search.

Returns:
    list[tuple[float, str]]: Suggested words with similarity scores.
)")
      .def("prefix_distance_search",
           &fstd::FstdxSearcher::prefix_distance_search, py::arg("word"),
           py::arg("names"), py::arg("max_distance") = 1,
           R"(Perform a prefix-distance search across multiple dictionaries.

Args:
    word: The word to search.
    names: List of dictionary names to search.
    max_distance: Maximum allowed prefix distance. Defaults to 1.

Returns:
    list[str]: Matching words within the prefix distance threshold.
)")
      .def("regex_search", &fstd::FstdxSearcher::regex_search,
           py::arg("pattern"), py::arg("names"),
           R"(Perform a regex search across multiple dictionaries.

Args:
    pattern: The regex pattern to match.
    names: List of dictionary names to search.

Returns:
    list[str]: Words matching the regex pattern.
)")
      .def("insert_prior_suffix", &fstd::FstdxSearcher::insert_prior_suffix,
           py::arg("sufs"),
           R"(Insert prior suffix rules into the searcher.

Args:
    sufs: List of prior suffixes to insert.

Returns:
    None.
)")
      .def("remove_all_prior_suffix",
           &fstd::FstdxSearcher::remove_all_prior_suffix,
           R"(Remove all prior suffix rules from the searcher.

Returns:
    None.
)")
      .def("resize_thread_pool", &fstd::FstdxSearcher::resize_thread_pool,
           py::arg("worker_num"),
           R"(Resize the number of threads for parallel search.
           
Args:
    worker_num: Number of threads for parallel search. Defaults to 0,
        which auto-detects available CPU threads.
    
Returns:
    None.
)")
      .def("insert_if_not_exists", &fstd::FstdxSearcher::insert_if_not_exists,
           py::arg("name"), py::arg("fstdx_path"),
           R"(Insert an fstdx dictionary only if it does not already exist.

Args:
    name: Name of the dictionary.
    fstdx_path: Path to the fstdx file.

Returns:
    None.
)")
      .def("insert", &fstd::FstdxSearcher::insert, py::arg("name"),
           py::arg("fstdx_path"),
           R"(Insert an fstdx dictionary into the searcher.

Args:
    name: Name of the dictionary.
    fstdx_path: Path to the fstdx file.

Returns:
    bool: True if insertion succeeds, False otherwise.
)")
      .def(
          "erase", &fstd::FstdxSearcher::erase, py::arg("name"),
          R"(Erase an fstdx dictionary and the corresponding fstdd archives from the searcher.

Args:
    name: Name of the dictionary.
    
Returns:
    None.
)")
      .def("save_to_disk", &fstd::FstdxSearcher::save_to_disk,
           py::arg("meta_json_path"),
           R"(Persist the current meta information to a JSON file on disk.

Args:
    meta_json_path: Path where the meta JSON file will be saved.

Returns:
    bool: True if saving succeeds, False otherwise.
)");
}