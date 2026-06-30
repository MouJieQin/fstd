#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>

#include "fstd/fstdx_reader.h"
// #include "fstd/fstdx_searcher.h"
// #include "fstd/fstdx_writer.h"

namespace py = pybind11;

PYBIND11_MODULE(_native, m) {
    m.doc() = "Python binding for fstd dictionary engine";

    py::class_<fstd::FstdxReader>(m, "FstdxReader")
        .def(py::init<const std::string&>(), py::arg("file_path"))
        .def("contains", &fstd::FstdxReader::contains, py::arg("word"));

    // py::class_<fstd::FstdxSearcher>(m, "FstdxSearcher")
    //     .def(py::init<>())
    //     .def("set_reader", &fstd::FstdxSearcher::set_reader)
    //     .def("prefix_search", &fstd::FstdxSearcher::prefix_search, py::arg("prefix"));

    m.def("get_version", [](){ return "0.1.0"; }, "Get fstd library version");
}