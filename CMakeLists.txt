cmake_minimum_required (VERSION 3.24) # Upgraded from 3.10 to unlock safe system fallback features

set(PYBIND11_FINDPYTHON OFF CACHE BOOL "" FORCE)
cmake_policy(SET CMP0148 NEW)

# Project configuration
project (fstd LANGUAGES CXX)
set(fstd_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

file(READ ${CMAKE_SOURCE_DIR}/VERSION FSTD_VERSION)
string(STRIP ${FSTD_VERSION} FSTD_VERSION)
set(FSTD_VERSION "${FSTD_VERSION}" CACHE STRING "Version from VERSION file" FORCE)
message(STATUS "fstd version: ${FSTD_VERSION}")

# Build type postfix configuration
set(RELEASE_POSTFIX "" CACHE STRING "Postfix for release build binaries")
set(DEBUG_POSTFIX "d" CACHE STRING "Postfix for debug build binaries")
set(COVERAGE_POSTFIX "" CACHE STRING "Postfix for coverage build binaries")
set(GPERF_POSTFIX "" CACHE STRING "Postfix for gperf build binaries")

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)

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
    string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)
endif()

# ================= Python Binding Switch =================
option(BUILD_PYTHON_BINDING "Build pybind11 python extension module" OFF)
# =========================================================

# CRITICAL FOR LINUX DYNAMIC LIBRARIES:
# Forces compiled modules from FetchContent (spdlog, fmt, zstd, pcre2) to use -fPIC 
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Require C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(GNUInstallDirs)
enable_testing()

# ===================== Library Configuration =====================
aux_source_directory(${fstd_ROOT_DIR}/lib/src DIR_LIB_SRCS)
set(FSTD_INCLUDE_DIR ${fstd_ROOT_DIR}/lib/include)

# 1. Build shared library
add_library(fstd_shared SHARED ${DIR_LIB_SRCS})
set_target_properties(fstd_shared PROPERTIES
    OUTPUT_NAME fstd${BUILD_TYPE}
    VERSION ${FSTD_VERSION}
    SOVERSION 1
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# 2. Build static library
add_library(fstd_static STATIC ${DIR_LIB_SRCS})
set_target_properties(fstd_static PROPERTIES
    OUTPUT_NAME fstd${BUILD_TYPE}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)
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

target_compile_definitions(fstd PRIVATE SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
target_compile_definitions(fstd_shared PRIVATE FSTD_VERSION="${FSTD_VERSION}")
target_compile_definitions(fstd_static PRIVATE FSTD_VERSION="${FSTD_VERSION}")
target_compile_definitions(fstd PRIVATE FSTD_VERSION="${FSTD_VERSION}")

if(WIN32)
    add_compile_definitions(NOMINMAX)
endif()

if(MSVC)
    set(COMMON_COMPILE_OPTIONS /W4)
else()
    set(COMMON_COMPILE_OPTIONS -W -Wall)
endif()

set(TARGETS fstd fstd_shared fstd_static)
foreach(TARGET ${TARGETS})
    # Apply standard warning options
    target_compile_options(${TARGET} PRIVATE ${COMMON_COMPILE_OPTIONS})

    # Apply optimizations based on build type
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


# ===================== AUTO-FALLBACK INDEPENDENT SYSTEM FINDERS =====================
include(FetchContent)

macro(find_shared_or_fetch NAME REPO TAG SUBDIR)
    set(USE_SYSTEM_LIB FALSE)
    if(UNIX AND NOT APPLE)
        find_library(${NAME}_SYSTEM_SHARED NAMES ${NAME} PATHS /usr/lib /usr/local/lib /usr/lib/x86_64-linux-gnu)
        if(${NAME}_SYSTEM_SHARED MATCHES "\\.so$|\\.so\\.")
            set(USE_SYSTEM_LIB TRUE)
        endif()
    else()
        find_package(${NAME} QUIET)
        if(${NAME}_FOUND)
            set(USE_SYSTEM_LIB TRUE)
        endif()
    endif()

    if(USE_SYSTEM_LIB)
        message(STATUS "--> Using system-provided shared library for: ${NAME}")
        if(${NAME} STREQUAL "pybind11")
            if(NOT TARGET pybind11::pybind11)
                add_library(pybind11::pybind11 ALIAS pybind11)
            endif()
        else()
            if(NOT TARGET ${NAME}::${NAME} AND TARGET ${NAME})
                # If the system target is an ALIAS, unpack the real target behind it
                get_target_property(REAL_SYS_TARGET ${NAME} ALIASED_TARGET)
                if(REAL_SYS_TARGET)
                    add_library(${NAME}::${NAME} ALIAS ${REAL_SYS_TARGET})
                else()
                    add_library(${NAME}::${NAME} ALIAS ${NAME})
                endif()
            elseif(NOT TARGET ${NAME}::${NAME})
                add_library(${NAME}_system INTERFACE IMPORTED)
                target_link_libraries(${NAME}_system INTERFACE ${${NAME}_SYSTEM_SHARED})
                add_library(${NAME}::${NAME} ALIAS ${NAME}_system)
            endif()
        endif()
    else()
        message(STATUS "--> System library missing or lacks -fPIC for ${NAME}. Compiling via FetchContent from GitHub...")
        if("${SUBDIR}" STREQUAL "")
            FetchContent_Declare(${NAME} GIT_REPOSITORY ${REPO} GIT_TAG ${TAG})
        else()
            FetchContent_Declare(${NAME} GIT_REPOSITORY ${REPO} GIT_TAG ${TAG} SOURCE_SUBDIR ${SUBDIR})
        endif()
        FetchContent_MakeAvailable(${NAME})
    endif()
endmacro()

# pybind11 for python binding
if(BUILD_PYTHON_BINDING)
    find_shared_or_fetch(pybind11 "https://github.com/pybind/pybind11.git" "v3.0.4" "")
endif()

# 1. indicators (progress bar)
find_shared_or_fetch(indicators "https://github.com/p-ranav/indicators.git" "v2.3" "")

# 2. nlohmann_json (JSON)
find_shared_or_fetch(nlohmann_json "https://github.com/nlohmann/json.git" "v3.12.0" "")

# 3. spdlog (log)
find_shared_or_fetch(spdlog "https://github.com/gabime/spdlog.git" "v1.17.0" "")

# 4. fmt
find_shared_or_fetch(fmt "https://github.com/fmtlib/fmt.git" "12.1.0" "")

# 5. CLI11 (Command line parser)
find_shared_or_fetch(CLI11 "https://github.com/CLIUtils/CLI11.git" "v2.6.2" "")

# 6. zstd (compression)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)

# Force macOS to fall back cleanly if Homebrew target linking drops dictionary symbols
if(APPLE)
    find_package(zstd QUIET)
    if(zstd_FOUND)
        # Verify if an absolute target definition or a raw variable link is available
        if(TARGET zstd::libzstd_shared)
            set(ZSTD_LIB_TARGET zstd::libzstd_shared)
        elseif(TARGET zstd::libzstd_static)
            set(ZSTD_LIB_TARGET zstd::libzstd_static)
        else()
            # Absolute fallback to Homebrew's framework library directory explicitly
            find_library(ZSTD_MAC_LIB NAMES zstd PATHS /opt/homebrew/lib /usr/local/lib)
            if(ZSTD_MAC_LIB)
                add_library(zstd_mac_fallback INTERFACE IMPORTED)
                target_link_libraries(zstd_mac_fallback INTERFACE ${ZSTD_MAC_LIB})
                set(ZSTD_LIB_TARGET zstd_mac_fallback)
            endif()
        endif()
        
        # Bind the localized workspace target to our namespace wrapper
        if(NOT TARGET zstd::zstd AND ZSTD_LIB_TARGET)
            add_library(zstd::zstd INTERFACE IMPORTED)
            target_link_libraries(zstd::zstd INTERFACE ${ZSTD_LIB_TARGET})
            set(zstd_RESOLVED TRUE)
        endif()
    endif()
endif()

# Fall back to standard macro tracking for Linux/Ubuntu builds or unresolved Mac systems
if(NOT zstd_RESOLVED)
    find_shared_or_fetch(zstd "https://github.com/facebook/zstd.git" "v1.5.7" "build/cmake")
    if(NOT TARGET zstd::zstd)
        if(TARGET zstd)
            add_library(zstd::zstd ALIAS zstd)
        elseif(TARGET libzstd_static)
            get_target_property(REAL_ZSTD_TARGET libzstd_static ALIASED_TARGET)
            if(REAL_ZSTD_TARGET)
                add_library(zstd::zstd ALIAS ${REAL_ZSTD_TARGET})
            else()
                add_library(zstd::zstd ALIAS libzstd_static)
            endif()
        endif()
    endif()
endif()

# 7. pcre2 (regex)
set(PCRE2_BUILD_PCRE2GREP OFF CACHE BOOL "" FORCE)
set(PCRE2_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(PCRE2_BUILD_PCRE2_8 ON CACHE BOOL "Build 8-bit PCRE2 library" FORCE)

set(pcre2_RESOLVED FALSE)

# Force macOS to fall back cleanly if Homebrew target linking drops 8-bit symbols
if(APPLE)
    find_package(pcre2 QUIET)
    find_package(PCRE2 QUIET) # Check both capitalization variants
    
    if(pcre2_FOUND OR PCRE2_FOUND)
        if(TARGET pcre2::8bit)
            set(PCRE2_LIB_TARGET pcre2::8bit)
        elseif(TARGET PCRE2::8BIT)
            set(PCRE2_LIB_TARGET PCRE2::8BIT)
        else()
            # Explicit physical fallback to Homebrew's framework library directory
            find_library(PCRE2_MAC_LIB NAMES pcre2-8 pcre2 PATHS /opt/homebrew/lib /usr/local/lib)
            if(PCRE2_MAC_LIB)
                add_library(pcre2_mac_fallback INTERFACE IMPORTED)
                target_link_libraries(pcre2_mac_fallback INTERFACE ${PCRE2_MAC_LIB})
                set(PCRE2_LIB_TARGET pcre2_mac_fallback)
            endif()
        endif()
        
        # Bind the localized workspace target to our namespace wrapper
        if(NOT TARGET pcre2::pcre2 AND PCRE2_LIB_TARGET)
            add_library(pcre2::pcre2 INTERFACE IMPORTED)
            target_link_libraries(pcre2::pcre2 INTERFACE ${PCRE2_LIB_TARGET})
            set(pcre2_RESOLVED TRUE)
        endif()
    endif()
endif()

# Fall back to standard macro tracking for Linux/Ubuntu builds or unresolved Mac systems
if(NOT pcre2_RESOLVED)
    find_shared_or_fetch(pcre2 "https://github.com/PhilipHazel/pcre2.git" "pcre2-10.47" "")
    if(NOT TARGET pcre2::pcre2)
        if(TARGET pcre2-8)
            get_target_property(REAL_PCRE2_TARGET pcre2-8 ALIASED_TARGET)
            if(REAL_PCRE2_TARGET)
                add_library(pcre2::pcre2 ALIAS ${REAL_PCRE2_TARGET})
            else()
                add_library(pcre2::pcre2 ALIAS pcre2-8)
            endif()
        elseif(TARGET PCRE2::8BIT)
            add_library(pcre2::pcre2 ALIAS PCRE2::8BIT)
        endif()
    endif()
endif()

# Link cross-platform dynamic system fallback dependencies
foreach(TARGET ${TARGETS})
    target_link_libraries(${TARGET} PUBLIC
        spdlog::spdlog
        fmt::fmt
        nlohmann_json::nlohmann_json
        indicators::indicators
        CLI11::CLI11
        zstd::zstd
        pcre2::pcre2
    )
endforeach()
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

# ================= Build Python Binding =================
if(BUILD_PYTHON_BINDING)
    add_subdirectory(python)
endif()
# =========================================================

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