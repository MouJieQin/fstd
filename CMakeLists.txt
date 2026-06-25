cmake_minimum_required (VERSION 3.10)

# 构建类型后缀配置
set(RELEASE_POSTFIX "" CACHE STRING "Postfix for release build binaries")
set(DEBUG_POSTFIX "d" CACHE STRING "Postfix for debug build binaries")
set(COVERAGE_POSTFIX "" CACHE STRING "Postfix for coverage build binaries")
set(GPERF_POSTFIX "" CACHE STRING "Postfix for gperf build binaries")

# 统一转小写
string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)

# 设置构建类型标识
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

# 项目配置
project (fstd LANGUAGES CXX)
set(fstd_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# C++20 强制标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 标准安装路径（兼容所有系统）
include(GNUInstallDirs)

# 开启测试
enable_testing()

# ===================== 库文件配置（核心：静态库 + 动态库） =====================
# 自动收集库源码
aux_source_directory(${fstd_ROOT_DIR}/lib/src DIR_LIB_SRCS)

# 库头文件路径
set(FSTD_INCLUDE_DIR ${fstd_ROOT_DIR}/lib/include)
include_directories(${FSTD_INCLUDE_DIR})

# 1. 编译动态库
add_library(fstd_shared SHARED ${DIR_LIB_SRCS})
set_target_properties(fstd_shared PROPERTIES
    OUTPUT_NAME fstd${BUILD_TYPE}
    VERSION 1.0.0
    SOVERSION 1
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# 2. 编译静态库
add_library(fstd_static STATIC ${DIR_LIB_SRCS})
set_target_properties(fstd_static PROPERTIES
    OUTPUT_NAME fstd${BUILD_TYPE}
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
)

# 给库设置头文件属性（安装时自动关联）
target_include_directories(fstd_shared PUBLIC
    $<BUILD_INTERFACE:${FSTD_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_include_directories(fstd_static PUBLIC
    $<BUILD_INTERFACE:${FSTD_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
# ============================================================================

# ===================== 可执行文件配置 =====================
add_executable(fstd ${fstd_ROOT_DIR}/src/main.cpp)
target_link_libraries(fstd PRIVATE fstd_static)  # 可执行文件链接静态库
# ==========================================================

# 编译宏定义
target_compile_definitions(fstd PRIVATE
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE
)

# 通用编译警告
set(COMMON_COMPILE_OPTIONS
    -W
    -Wall
)

# 按构建类型设置编译选项（库 + 可执行文件统一配置）
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

# 第三方库链接（库 + 可执行文件统一链接）
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

# ===================== GTest 混合模式：优先系统/brew，失败则自动下载 =====================
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
      # 禁止 gtest 安装，避免和系统库冲突
      EXCLUDE_FROM_ALL
    )
    # 关闭 gtest 自身测试，加快编译
    set(gtest_build_tests OFF CACHE BOOL "" FORCE)
    set(gmock_build_tests OFF CACHE BOOL "" FORCE)
    # 新增：关闭 gtest 内部依赖检查（进一步防重复）
    set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

# 加载测试子目录
add_subdirectory(tests)
# ========================================================================================

# 覆盖率链接配置
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

# ===================== 安装配置（核心） =====================
# 1. 安装可执行文件
install(TARGETS fstd
    DESTINATION ${CMAKE_INSTALL_BINDIR}
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

# 2. 安装库文件（静态 + 动态）
install(TARGETS fstd_shared fstd_static
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}  # 动态库
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}  # 静态库
)

# 3. 安装头文件（安装后路径：/usr/local/include/fstd/xxx.hpp）
install(DIRECTORY ${FSTD_INCLUDE_DIR}/fstd ${FSTD_INCLUDE_DIR}/fstlib
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
)

# ===================== 卸载配置（make uninstall） =====================
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