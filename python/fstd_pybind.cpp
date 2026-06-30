#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>

#include "fstd/fstdd_reader.h"
#include "fstd/fstdd_writer.h"
#include "fstd/fstdx_reader.h"
#include "fstd/fstdx_searcher.h"
#include "fstd/fstdx_writer.h"

namespace py = pybind11;

PYBIND11_MODULE(_native, m) {
  m.doc() = "Python binding for fstd dictionary engine";

  py::class_<fstd::FstdxReader>(m, "FstdxReader")
      .def(py::init<const std::string &>(), py::arg("fstdx_path"),
           R"(Initialize the reader with fstdx_path.
            :param fstdx_path: the path to the fstdx file
           )")
      .def("get_fst_key_size", &fstd::FstdxReader::get_fst_key_size,
           R"(Get the key size of the fst index.
            :return: the key size of the fst index
           )")
      .def("contains", &fstd::FstdxReader::contains, py::arg("word"),
           R"(Check if the word is in the dictionary.
            :param word: the word to check
            :return: True if the word is in the dictionary, False otherwise
           )");

  py::class_<fstd::FstdxWriter>(m, "FstdxWriter")
      .def(py::init<>())
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
              :param compress_level: the compress level
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
            :default worker_num is 0, automatically use all the current threads
           )")
      .def("contains", &fstd::FstdxSearcher::contains, py::arg("word"),
           py::arg("names"),
           R"(Check if the word is in the dictionary.
            :param word: the word to check
            :param names: the names of dictionaries to check
            :return: True if the word is in the dictionaries, False otherwise
           )");

  m.def("get_version", []() { return "0.1.0"; }, "Get fstd library version");
}