cmake_minimum_required (VERSION 3.10)

# 构建类型后缀配置
set(RELEASE_POSTFIX "" CACHE STRING "Postfix for release build binaries")
set(DEBUG_POSTFIX "d" CACHE STRING "Postfix for debug build binaries")
set(COVERAGE_POSTFIX "" CACHE STRING "Postfix for coverage build binaries")
set(GPERF_POSTFIX "" CACHE STRING "Postfix for gperf build binaries")

# to lower case
string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)

# build type postfix
if (CMAKE_BUILD_TYPE_LOWER STREQUAL "release")
    set(BUILD_TYPE ${RELEASE_POSTFIX})
elseif (CMAKE_BUILD_TYPE_LOWER STREQUAL "debug")
    set(BUILD_TYPE ${DEBUG_POSTFIX})
elseif(CMAKE_BUILD_TYPE_LOWER STREQUAL "coverage")
    set(BUILD_TYPE ${COVERAGE_POSTFIX})
elseif(CMAKE_BUILD_TYPE_LOWER STREQUAL "gperf")
    set(BUILD_TYPE ${GPERF_POSTFIX})
else()
    message(WARNING "Unknown build type '${CMAKE_BUILD_TYPE}', defaulting to release")
    set(BUILD_TYPE ${RELEASE_POSTFIX})
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

# project configuration
project (fstd LANGUAGES CXX)
set(fstd_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# C++20 standard required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# standard install path (compatible with all systems)
include(GNUInstallDirs)

enable_testing()

# ===================== library configuration =====================
# automatically collect library source files
aux_source_directory(${fstd_ROOT_DIR}/lib/src DIR_LIB_SRCS)

# library header path
set(FSTD_INCLUDE_DIR ${fstd_ROOT_DIR}/lib/include)
include_directories(${FSTD_INCLUDE_DIR})

# 1. compile shared library
add_library(fstd_shared SHARED ${DIR_LIB_SRCS})
set_target_properties(fstd_shared PROPERTIES
    OUTPUT_NAME fstd${BUILD_TYPE}
    VERSION 1.0.0
    SOVERSION 1
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# 2. compile static library
add_library(fstd_static STATIC ${DIR_LIB_SRCS})
set_target_properties(fstd_static PROPERTIES
    OUTPUT_NAME fstd${BUILD_TYPE}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# set library header path
target_include_directories(fstd_shared PUBLIC
    $<BUILD_INTERFACE:${FSTD_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_include_directories(fstd_static PUBLIC
    $<BUILD_INTERFACE:${FSTD_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
# ============================================================================

# ===================== executable configuration =====================
add_executable(fstd ${fstd_ROOT_DIR}/src/main.cpp)
target_link_libraries(fstd PRIVATE fstd_static)  # executable links static library
# ==========================================================

# compile macro definitions
target_compile_definitions(fstd PRIVATE
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE
)

# common compile warnings
set(COMMON_COMPILE_OPTIONS
    -W
    -Wall
)

# set compile options for each target
set(TARGETS fstd fstd_shared fstd_static)
foreach(TARGET ${TARGETS})
    target_compile_options(${TARGET} PRIVATE ${COMMON_COMPILE_OPTIONS})
    if (CMAKE_BUILD_TYPE_LOWER STREQUAL "release")
        target_compile_options(${TARGET} PRIVATE -O2)
    elseif (CMAKE_BUILD_TYPE_LOWER STREQUAL "debug")
        target_compile_options(${TARGET} PRIVATE -O0 -g -DDEBUG)
    elseif (CMAKE_BUILD_TYPE_LOWER STREQUAL "coverage")
        target_compile_options(${TARGET} PRIVATE -O0 -g --coverage)
    elseif (CMAKE_BUILD_TYPE_LOWER STREQUAL "gperf")
        target_compile_options(${TARGET} PRIVATE -O2 -g -DGPERF)
    endif()
endforeach()

# link third-party libraries
find_package(nlohmann_json REQUIRED)
foreach(TARGET ${TARGETS})
    target_link_libraries(${TARGET} PRIVATE
        spdlog
        fmt
        zstd
        pcre2-8
        nlohmann_json::nlohmann_json
    )
endforeach()

# ===================== GTest configuration =====================
find_package(GTest QUIET)

if(GTest_FOUND)
    message(STATUS "Found system/brew installed Google Test, use it.")
else()
    message(STATUS "Google Test not found locally, will download and build from source...")
    include(FetchContent)
    FetchContent_Declare(
      googletest
      GIT_REPOSITORY https://github.com/google/googletest.git
      GIT_TAG        v1.17.0
      # disable installing gtest library
      EXCLUDE_FROM_ALL
    )
    # disable gtest self-tests
    set(gtest_build_tests OFF CACHE BOOL "" FORCE)
    set(gmock_build_tests OFF CACHE BOOL "" FORCE)
    # disable gtest internal dependencies
    set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

# load test subdirectory
add_subdirectory(tests)
# ========================================================================================

# coverage report
if (CMAKE_BUILD_TYPE_LOWER STREQUAL "coverage")
    foreach(TARGET ${TARGETS})
        target_link_libraries(${TARGET} PRIVATE --coverage -fprofile-arcs -ftest-coverage)
    endforeach()

    add_custom_target(coverage
        COMMAND lcov --base-directory ${CMAKE_SOURCE_DIR} --directory ${CMAKE_BINARY_DIR} --capture --output-file CodeCoverage.info --ignore-errors graph --rc lcov_branch_coverage=1
        COMMAND lcov --remove CodeCoverage.info '/Library/*' '/usr/*' '*/test/*' --output-file CodeCoverage.info.cleaned --rc lcov_branch_coverage=1
        COMMAND genhtml --branch-coverage CodeCoverage.info.cleaned -o CodeCoverageReport --rc lcov_branch_coverage=1
        DEPENDS fstd
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating code coverage report"
    )

    add_custom_target(open_coverage
        COMMAND open ${CMAKE_BINARY_DIR}/CodeCoverageReport/index.html
        DEPENDS coverage
        COMMENT "Opening coverage report"
    )
endif()

# gperf 性能分析
if (CMAKE_BUILD_TYPE_LOWER STREQUAL "gperf")
    foreach(TARGET ${TARGETS})
        target_link_libraries(${TARGET} PRIVATE -lprofiler)
    endforeach()
endif()

# ===================== install configuration =====================
# 1. install executable file
install(TARGETS fstd
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

# 2. install library files (static + dynamic)
install(TARGETS fstd_shared fstd_static
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}  # 动态库
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}  # 静态库
)

# 3. install header files
install(DIRECTORY ${FSTD_INCLUDE_DIR}/fstd ${FSTD_INCLUDE_DIR}/fstlib
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# ===================== uninstall configuration =====================
if(NOT TARGET uninstall)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY
    )

    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake
        COMMENT "Uninstalling fstd..."
    )
endif()