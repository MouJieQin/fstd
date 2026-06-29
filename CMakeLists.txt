cmake_minimum_required (VERSION 3.10)

# Build type postfix configuration
set(RELEASE_POSTFIX "" CACHE STRING "Postfix for release build binaries")
set(DEBUG_POSTFIX "d" CACHE STRING "Postfix for debug build binaries")
set(COVERAGE_POSTFIX "" CACHE STRING "Postfix for coverage build binaries")
set(GPERF_POSTFIX "" CACHE STRING "Postfix for gperf build binaries")

# Convert build type to lower case
string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)

# Set build type postfix
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

# Project configuration
project (fstd LANGUAGES CXX)
set(fstd_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Require C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Standard installation paths (cross-platform)
include(GNUInstallDirs)

# Enable testing
enable_testing()

# ===================== Library Configuration =====================
# Collect source files for the library
aux_source_directory(${fstd_ROOT_DIR}/lib/src DIR_LIB_SRCS)

# Library public include directory
set(FSTD_INCLUDE_DIR ${fstd_ROOT_DIR}/lib/include)
include_directories(${FSTD_INCLUDE_DIR})

# 1. Build shared library
add_library(fstd_shared SHARED ${DIR_LIB_SRCS})
set_target_properties(fstd_shared PROPERTIES
    OUTPUT_NAME fstd${BUILD_TYPE}
    VERSION 1.0.0
    SOVERSION 1
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# 2. Build static library
add_library(fstd_static STATIC ${DIR_LIB_SRCS})
set_target_properties(fstd_static PROPERTIES
    OUTPUT_NAME fstd${BUILD_TYPE}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# Set include directories for both library targets
target_include_directories(fstd_shared PUBLIC
    $<BUILD_INTERFACE:${FSTD_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_include_directories(fstd_static PUBLIC
    $<BUILD_INTERFACE:${FSTD_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
# ============================================================================

# ===================== Executable Configuration =====================
add_executable(fstd ${fstd_ROOT_DIR}/src/main.cpp)
target_link_libraries(fstd PRIVATE fstd_static)
# ============================================================================

# Compile definitions
target_compile_definitions(fstd PRIVATE
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE
)

# Common warning flags
if(MSVC)
    set(COMMON_COMPILE_OPTIONS /W4)
else()
    set(COMMON_COMPILE_OPTIONS -W -Wall)
endif()

# Apply compile options to all targets
set(TARGETS fstd fstd_shared fstd_static)
foreach(TARGET ${TARGETS})
    target_compile_options(${TARGET} PRIVATE ${COMMON_COMPILE_OPTIONS})

    if (CMAKE_BUILD_TYPE_LOWER STREQUAL "release")
        if(MSVC)
            target_compile_options(${TARGET} PRIVATE /O2)
        else()
            target_compile_options(${TARGET} PRIVATE -O2)
        endif()
    elseif (CMAKE_BUILD_TYPE_LOWER STREQUAL "debug")
        if(MSVC)
            target_compile_options(${TARGET} PRIVATE /Od /Zi /DDEBUG)
        else()
            target_compile_options(${TARGET} PRIVATE -O0 -g -DDEBUG)
        endif()
    endif()
endforeach()

# ===================== Auto-download all cross-platform dependencies =====================
include(FetchContent)

# 1. indicators (progress bar)
find_package(indicators QUIET)
if(NOT indicators_FOUND)
    message(STATUS "indicators not found, downloading from GitHub...")
    FetchContent_Declare(
        indicators
        GIT_REPOSITORY https://github.com/p-ranav/indicators.git
        GIT_TAG        v2.3
    )
    FetchContent_MakeAvailable(indicators)
endif()

# 2. nlohmann_json (JSON)
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found, downloading from GitHub...")
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.12.0
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# 3. spdlog (log)
find_package(spdlog QUIET)
if(NOT spdlog_FOUND)
    message(STATUS "spdlog not found, downloading...")
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        v1.17.0
    )
    FetchContent_MakeAvailable(spdlog)
endif()

# 4. fmt
find_package(fmt QUIET)
if(NOT fmt_FOUND)
    message(STATUS "fmt not found, downloading...")
    FetchContent_Declare(
        fmt
        GIT_REPOSITORY https://github.com/fmtlib/fmt.git
        GIT_TAG        12.1.0
    )
    FetchContent_MakeAvailable(fmt)
endif()

# 5. zstd (compression)
find_package(zstd QUIET)
if(NOT zstd_FOUND)
    message(STATUS "zstd not found, downloading...")
    FetchContent_Declare(
        zstd
        GIT_REPOSITORY https://github.com/facebook/zstd
        GIT_TAG        v1.5.7
        SOURCE_SUBDIR  build/cmake
    )
    FetchContent_MakeAvailable(zstd)
endif()

# 6. pcre2 (regex)
find_package(PCRE2 QUIET)
if(NOT PCRE2_FOUND)
    message(STATUS "pcre2 not found, downloading...")
    FetchContent_Declare(
        pcre2
        GIT_REPOSITORY https://github.com/PhilipHazel/pcre2
        GIT_TAG        pcre2-10.47
    )
    FetchContent_MakeAvailable(pcre2)
endif()

# Link cross-platform dependencies
foreach(TARGET ${TARGETS})
    target_link_libraries(${TARGET} PRIVATE
        spdlog::spdlog
        fmt::fmt
        nlohmann_json::nlohmann_json
        indicators::indicators
        zstd$<$<CONFIG:Debug>:d>  #debug/release
        pcre2-8
    )
endforeach()

# Link Linux-only libraries (zstd, pcre2-8) only on Linux
if(UNIX AND NOT APPLE)
    find_package(zstd QUIET)
    if(NOT zstd_FOUND)
        message(STATUS "zstd not found, installing via system package...")
    endif()

    find_package(Pcre2 QUIET)
    if(NOT Pcre2_FOUND)
        message(STATUS "pcre2 not found, installing via system package...")
    endif()

    foreach(TARGET ${TARGETS})
        target_link_libraries(${TARGET} PRIVATE zstd pcre2-8)
    endforeach()
endif()
# ============================================================================

# ===================== Google Test Configuration =====================
find_package(GTest QUIET)
if(GTest_FOUND)
    message(STATUS "Found system-installed GTest, using it.")
else()
    message(STATUS "GTest not found, downloading from source...")
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.17.0
        EXCLUDE_FROM_ALL
    )
    set(gtest_build_tests OFF CACHE BOOL "" FORCE)
    set(gmock_build_tests OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

# Include test directory
add_subdirectory(tests)
# ============================================================================

# ===================== Coverage (Linux only) =====================
if (CMAKE_BUILD_TYPE_LOWER STREQUAL "coverage" AND UNIX AND NOT APPLE)
    foreach(TARGET ${TARGETS})
        target_compile_options(${TARGET} PRIVATE --coverage)
        target_link_libraries(${TARGET} PRIVATE --coverage)
    endforeach()

    add_custom_target(coverage
        COMMAND lcov --base-directory ${CMAKE_SOURCE_DIR} --directory ${CMAKE_BINARY_DIR} --capture --output-file CodeCoverage.info --ignore-errors graph --rc lcov_branch_coverage=1
        COMMAND lcov --remove CodeCoverage.info '/Library/*' '/usr/*' '*/test/*' --output-file CodeCoverage.info.cleaned --rc lcov_branch_coverage=1
        COMMAND genhtml --branch-coverage CodeCoverage.info.cleaned -o CodeCoverageReport --rc lcov_branch_coverage=1
        DEPENDS fstd
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Generating code coverage report"
    )
endif()

# ===================== gperf (Linux only) =====================
if (CMAKE_BUILD_TYPE_LOWER STREQUAL "gperf" AND UNIX AND NOT APPLE)
    foreach(TARGET ${TARGETS})
        target_link_libraries(${TARGET} PRIVATE -lprofiler)
    endforeach()
endif()

# ===================== Install Configuration =====================
# Install executable
install(TARGETS fstd
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Install libraries
install(TARGETS fstd_shared fstd_static
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# Install headers
install(DIRECTORY ${FSTD_INCLUDE_DIR}/fstd ${FSTD_INCLUDE_DIR}/fstlib
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# ===================== Uninstall Target =====================
if(NOT TARGET uninstall)
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
        "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake"
        IMMEDIATE @ONLY
    )

    add_custom_target(uninstall
        COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/cmake_uninstall.cmake
        COMMENT "Uninstalling fstd..."
    )
endif()
