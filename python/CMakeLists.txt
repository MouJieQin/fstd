cmake_minimum_required(VERSION 3.24)

pybind11_add_module(_native SHARED fstd_pybind.cpp)

target_compile_definitions(_native PRIVATE FSTD_VERSION="${FSTD_VERSION}")

target_include_directories(_native PRIVATE
    ${fstd_ROOT_DIR}/lib/include
)

target_link_libraries(_native PRIVATE fstd_static)

if(MSVC)
    target_compile_options(_native PRIVATE /W4 /O2)
else()
    target_compile_options(_native PRIVATE -W -Wall -O2)
endif()

target_compile_features(_native PRIVATE cxx_std_20)