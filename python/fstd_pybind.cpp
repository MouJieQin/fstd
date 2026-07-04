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
      "get_version", []() { return FSTD_VERSION; }, "Get fstd library version");

  m.def(
      "set_log_level",
      [](uint8_t log_level) {
        Logger::instance().set_level(
            static_cast<spdlog::level::level_enum>(log_level));
      },
      "Set the log level for fstd library. log_level: 0-6, 0 is trace, 1 is "
      "debug, 2 is info, 3 is warn, 4 is error, 5 is critical, 6 is off");

  py::class_<fstd::FstddReader>(m, "FstddReader")
      .def(py::init<const std::string &>(), py::arg("output_file"),
           R"(Initialize the reader with output_file.
            :param output_file: the path to the fstdd file
           )")
      .def("is_valid", &fstd::FstddReader::operator bool,
           R"(Check if the fstdd file is valid.
            :return: True if the fstdd file is valid, False otherwise
           )")
      .def(
          "get_meta",
          [](fstd::FstddReader &self) { return self.get_meta().dump(); },
          R"(Get the meta of the fstdd file.
             :return: the meta json string
            )")
      .def(
          "get_header",
          [](fstd::FstddReader &self) { return self.get_header().dump(); },
          R"(Get the header of the fstdd file.
             :return: the header json string
            )")
      .def("contains", &fstd::FstddReader::contains, py::arg("word"),
           R"(Check if the word is in the fstdd file.
             :param word: the word to check
             :return: True if the word is in the fstdd file, False otherwise
            )")
      .def(
          "extract_all_key",
          [](fstd::FstddReader &self) {
            std::vector<std::string> all_keys;
            self.extract_all_key(all_keys);
            return all_keys;
          },
          R"(Extract all keys from the fstdd file.
            :return: a vector of keys
           )")
      .def("extract", &fstd::FstddReader::extract, py::arg("key"),
           py::arg("dst_dir") = "data",
           R"(Extract the file with key to dst_dir.
            :param key: the key to extract
            :param dst_dir: the path to the destination directory, default is data
            :return: True if the extraction is successful, False otherwise
           )")
      .def("extract_all", &fstd::FstddReader::extract_all, py::arg("dst_dir"),
           R"(Extract all files in the fstdd file to dst_dir.
            :param dst_dir: the path to the destination directory
            :return: True if the extraction is successful, False otherwise
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
          R"(Compile the fstd file from data path.
            :param data_path: the path to the data file or directory
            :param output_file: the path to the output fstd file
            :param meta_json_str: the meta json string
            :param block_size_kb: the block size in kb
            :param compress_level: the compress level [0, 22]
            :param worker_num: the number of threads to use for compile
            :param opt_verbose: whether to print verbose info
            :return: True if the compilation is successful, False otherwise
           )")
      .def(
          "push_file_stream",
          [](fstd::FstddWriter &self, const std::string &file_path,
             py::bytes data) {
            std::string_view buf(data);
            return self.push_file_stream(file_path, buf);
          },
          py::arg("file_path"), py::arg("data"),
          R"(Push a file stream to the writer.
            :param file_path: the path(key) to the file to push
            :param data: the data of the file to push
            :return: True if the push is successful, False otherwise
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
          R"(Compile the fstd file from pushed file streams.
            :param file_stream_num: the number of file streams to compile
            :param output_file: the path to the output fstd file
            :param meta_json_str: the meta json string
            :param block_size_kb: the block size in kb
            :param compress_level: the compress level [0, 22]
            :param worker_num: the number of threads to use for compile
            :param opt_verbose: whether to print verbose info
            :return: True if the compilation is successful, False otherwise
           )");

  py::class_<fstd::FstdxReader>(m, "FstdxReader")
      .def(py::init<const std::string &>(), py::arg("fstdx_path"),
           R"(Initialize the reader with fstdx_path.
            :param fstdx_path: the path to the fstdx file
           )")
      .def("is_valid", &fstd::FstdxReader::operator bool,
           R"(Check if the fstdx file is valid.
            :return: True if the fstdx file is valid, False otherwise
           )")
      .def(
          "get_meta",
          [](fstd::FstdxReader &self) { return self.get_meta().dump(); },
          R"(Get the meta of the fstdx file.
             :return: the meta json string
            )")
      .def(
          "get_header",
          [](fstd::FstdxReader &self) { return self.get_header().dump(); },
          R"(Get the header of the fstdx file.
             :return: the header json string
            )")
      .def("get_key_size", &fstd::FstdxReader::get_key_size,
           R"(Get the key size of the entry words.
            :return: the key size of the entry words
           )")
      .def(
          "get_fst_key_size", &fstd::FstdxReader::get_fst_key_size,
          R"(Get the key size of the fst index. It's less or equal to the key size of the entry words because of duplicates.
            :return: the key size of the fst index
           )")
      .def(
          "extract_values",
          [](fstd::FstdxReader &self) { return self.extract_values(); },
          R"(Extract all values of the dictionary.
            :return: the values of the dictionary
           )")
      .def("contains", &fstd::FstdxReader::contains, py::arg("word"),
           R"(Check if the word is in the dictionary.
         :param word: the word to check
         :return: True if the word is in the dictionary, False otherwise
        )")
      .def(
          "exact_match_search",
          [](fstd::FstdxReader &self, const std::string &word) {
            std::vector<std::string> result;
            self.exact_match_search(word, result);
            return result;
          },
          py::arg("word"),
          R"(Search the exact match of the word in the dictionary.
         :param word: the word to search
         :return: the value of the word in the dictionary if the word is in
         the dictionary, otherwise an empty vector
        )")
      .def(
          "common_prefix_search",
          [](fstd::FstdxReader &self, const std::string &word) {
            return convert(self.common_prefix_search(word));
          },
          py::arg("word"),
          R"(Search the common prefix of the word in the dictionary.
          :param word: the word to search
          :return: the common prefix of the word in the dictionary if the
          word is in the dictionary, otherwise an empty vector
         )")
      .def("longest_prefix_len", &fstd::FstdxReader::longest_prefix_len,
           py::arg("word"),
           R"(Get the longest prefix length of the word in the dictionary.
            :param word: the word to search
            :return: the longest prefix length of the word in the dictionary.
        )")
      .def(
          "predictive_search",
          [](fstd::FstdxReader &self, const std::string &word) {
            return convert(self.predictive_search(word));
          },
          py::arg("word"),
          R"(Search the predictive of the word as a prefix in the dictionary.
          :param word: the word as a prefix to search
          :return: the predictive of the word as a prefix in the dictionary if the
          prefix is in the dictionary, otherwise an empty vector
         )")
      .def(
          "edit_distance_search",
          [](fstd::FstdxReader &self, const std::string &word,
             size_t distance) {
            return convert(self.edit_distance_search(word, distance));
          },
          py::arg("word"), py::arg("distance") = 1,
          R"(Search the edit distance of the word in the dictionary.
                      :param word: the word to search
                      :param distance: the edit distance to search
                      :return: the words that have an edit distance less than equal to the distance from the word in the dictionary
                     )")
      .def(
          "suggest",
          [](fstd::FstdxReader &self, const std::string &word) {
            return convert(self.suggest(word));
          },
          py::arg("word"),
          R"(Suggest the word in the dictionary.
                      :param word: the word to suggest
                      :return: the words that are suggested according to the word in the dictionary
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
          R"(Search the regex of the word in the dictionary.
          :param pattern: the regex pattern to search
          :param thread: the number of threads to use
          :return: the words that match the regex pattern in the dictionary
         )")
      .def(
          "spellcheck_word",
          [](fstd::FstdxReader &self, const std::string &word, size_t limit) {
            return convert(self.spellcheck_word(word, limit));
          },
          py::arg("word"), py::arg("limit") = 10,
          R"(Spellcheck the word in the dictionary.
          :param word: the word to spellcheck
          :param limit: the number of suggestions to return
          :return: the spellchecked word in the dictionary
         )")
      .def("enumerate_print", &fstd::FstdxReader::enumerate_print,
           R"(Print the dictionary to the console.
         :return: None
        )")
      .def("extract", &fstd::FstdxReader::extract, py::arg("output_file"),
           R"(Extract the raw text of the dictionary to the output file.
          :param output_file: the path to the output file
          :return: True if the dictionary is extracted, False otherwise
         )")
      .def(
          "extract_keys",
          [](fstd::FstdxReader &self) -> std::vector<std::string> {
            return self.extract_keys();
          },
          R"(Extract all keys of the dictionary.
          :return: the keys of the dictionary
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
          R"(Compile the fstdx file.
            :param input_file: the path to the input fstdx file
            :param output_file: the path to the output fstdx file
            :param meta_json_str: the meta json string
            :param block_size_kb: the block size in kb
            :param compress_level: the compress level [0, 22]
            :param zstd_dict_size_kb: the zstd dict size in kb
            :param worker_num: the number of threads to use for compile
            :param opt_sorted: whether to sort the values
            :param opt_verbose: whether to print verbose info
            :return: True if the compilation is successful, False otherwise
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
          R"(Compile the fstdx file.
              :param output_file: the path to the output fstdx file
              :param keys: the keys to compile
              :param values: the values to compile
              :param meta_json_str: the meta json string
              :param block_size_kb: the block size in kb
              :param compress_level: the compress level [0, 22]
              :param zstd_dict_size_kb: the zstd dict size in kb
              :param worker_num: the number of threads to use for compile
              :param opt_sorted: whether to sort the values
              :param opt_verbose: whether to print verbose info
              :return: True if the compilation is successful, False otherwise
             )");

  py::class_<fstd::FstdxSearcher>(m, "FstdxSearcher")
      .def(py::init<size_t>(), py::arg("worker_num") = 0,
           R"(Initialize the searcher with worker_num.
            :param worker_num: the number of threads to use for search
            :default worker_num is 0, automatically use all the current threads
           )")
      .def(py::init<const std::string &, size_t>(), py::arg("meta_json_path"),
           py::arg("worker_num") = 0,
           R"(Initialize the searcher with meta_json_path and worker_num.
              :param meta_json_path: the path to the meta json file
              :param worker_num: the number of threads to use for search
              :default worker_num is 0, automatically use all the current
              threads
             )")
      .def("is_valid", &fstd::FstdxSearcher::operator bool,
           R"(Check if the searcher is valid.
            :return: True if the searcher is valid, False otherwise
           )")
      .def(
          "extract",
          [](fstd::FstdxSearcher &self, const std::string &name,
             const std::string &file_path, const std::string &dst_dir) {
            if (dst_dir.empty()) { return self.extract(name, file_path); }
            return self.extract(name, file_path, dst_dir);
          },
          py::arg("name"), py::arg("file_path"), py::arg("dst_dir") = "",
          R"(Extract the fstdx file.
            :param name: the name of the dictionary
            :param file_path: the path(key) to the file to extract
            :param dst_dir: the destination directory to extract the files, if empty, will extract to the default directory
            :return: True if the extraction is successful, False otherwise
           )")
      .def("contains", &fstd::FstdxSearcher::contains, py::arg("word"),
           py::arg("names"),
           R"(Check if the word is in the dictionary.
            :param word: the word to check
            :param names: the names of dictionaries to check
            :return: True if the word is in the dictionaries, False otherwise
           )")
      .def(
          "exact_match_search",
          [](fstd::FstdxSearcher &self, const std::string &word,
             const std::string &name) {
            return self.exact_match_search(word, name);
          },
          py::arg("word"), py::arg("name"),
          R"(Search the word in the dictionary.
            :param word: the word to search
            :param name: the name of the dictionary to search
            :return: the results of the search
           )")
      .def(
          "exact_match_search",
          [](fstd::FstdxSearcher &self, const std::string &word,
             const std::vector<std::string> &names) {
            return self.exact_match_search(word, names);
          },
          py::arg("word"), py::arg("names"),
          R"(Search the word in the dictionaries.
            :param word: the word to search
            :param names: the names of dictionaries to search
            :return: the results of the search
           )")
      .def("common_prefix_search", &fstd::FstdxSearcher::common_prefix_search,
           py::arg("word"), py::arg("names"),
           R"(Search the common prefix of the word in the dictionaries.
            :param word: the word to search
            :param names: the names of dictionaries to search
            :return: the results of the search
           )")
      .def("longest_common_prefix_search",
           &fstd::FstdxSearcher::longest_common_prefix_search, py::arg("word"),
           py::arg("names"),
           R"(Search the longest common prefix of the word in the dictionaries.
            :param word: the word to search
            :param names: the names of dictionaries to search
            :return: the results of the search
           )")
      .def("edit_distance_search", &fstd::FstdxSearcher::edit_distance_search,
           py::arg("word"), py::arg("names"), py::arg("edit_distance") = 1,
           R"(Search the word in the dictionaries with edit distance.
            :param word: the word to search
            :param names: the names of dictionaries to search
            :param edit_distance: the maximum edit distance
            :return: the results of the search
           )")
      .def("predictive_search", &fstd::FstdxSearcher::predictive_search,
           py::arg("word"), py::arg("names"),
           R"(Perform predictive search for the word in the dictionaries.
            :param word: the word to search
            :param names: the names of dictionaries to search
            :return: the results of the search
           )")
      .def("suggest", &fstd::FstdxSearcher::suggest, py::arg("word"),
           py::arg("names"),
           R"(Provide suggestions for the word in the dictionaries.
            :param word: the word to search
            :param names: the names of dictionaries to search
            :return: the suggested words
           )")
      .def("prefix_distance_search",
           &fstd::FstdxSearcher::prefix_distance_search, py::arg("word"),
           py::arg("names"), py::arg("max_distance") = 1,
           R"(Search the word in the dictionaries with prefix distance.
            :param word: the word to search
            :param names: the names of dictionaries to search
            :param max_distance: the maximum prefix distance
            :return: the results of the search
           )")
      .def("regex_search", &fstd::FstdxSearcher::regex_search,
           py::arg("pattern"), py::arg("names"),
           R"(Search the word in the dictionaries with regex.
            :param pattern: the regex pattern to search
            :param names: the names of dictionaries to search
            :return: the results of the search
           )")
      .def("insert_prior_suffix", &fstd::FstdxSearcher::insert_prior_suffix,
           py::arg("sufs"),
           R"(Insert prior suffixes for the dictionaries.
            :param sufs: the prior suffixes to insert
            :return: None
           )")
      .def("insert_if_not_exists", &fstd::FstdxSearcher::insert_if_not_exists,
           py::arg("name"), py::arg("fstdx_path"),
           R"(Insert the fstdx file if it does not exist.
            :param name: the name of the dictionary
            :param fstdx_path: the path to the fstdx file
            :return: True if the insertion is successful, False otherwise
           )")
      .def("insert", &fstd::FstdxSearcher::insert, py::arg("name"),
           py::arg("fstdx_path"),
           R"(Insert the fstdx file.
            :param name: the name of the dictionary
            :param fstdx_path: the path to the fstdx file
            :return: True if the insertion is successful, False otherwise
           )")
      .def("save_to_disk", &fstd::FstdxSearcher::save_to_disk,
           py::arg("meta_json_path"),
           R"(Save the meta json to disk.
            :param meta_json_path: the path to save the meta json
            :return: True if the save is successful, False otherwise
           )");
}