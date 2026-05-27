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

# ===================== 核心修改：目录结构匹配 =====================
# 包含头文件路径：lib/include 放你的库头文件
include_directories(${fstd_ROOT_DIR}/lib/include)

# 可执行文件：src/main.cpp
add_executable(fstd
    ${fstd_ROOT_DIR}/src/main.cpp
)

# 自动收集 lib/src/ 下所有库源码
aux_source_directory(${fstd_ROOT_DIR}/lib/src DIR_LIB_SRCS)
target_sources(fstd PRIVATE ${DIR_LIB_SRCS})
# =================================================================

target_compile_definitions(fstd PRIVATE
    SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE
)

# 通用编译警告
set(COMMON_COMPILE_OPTIONS
    -W
    -Wall
)

# 按构建类型设置编译选项
if (CMAKE_BUILD_TYPE_LOWER STREQUAL "release")
    target_compile_options(fstd PRIVATE
        ${COMMON_COMPILE_OPTIONS}
        -O2
    )
elseif (CMAKE_BUILD_TYPE_LOWER STREQUAL "debug")
    target_compile_options(fstd PRIVATE
        ${COMMON_COMPILE_OPTIONS}
        -O0
        -g
        -DDEBUG
    )
elseif (CMAKE_BUILD_TYPE_LOWER STREQUAL "coverage")
    target_compile_options(fstd PRIVATE
        ${COMMON_COMPILE_OPTIONS}
        -O0
        -g
        --coverage
    )
elseif (CMAKE_BUILD_TYPE_LOWER STREQUAL "gperf")
    target_compile_options(fstd PRIVATE
        ${COMMON_COMPILE_OPTIONS}
        -O2
        -g
        -DGPERF
    )
endif()

# 第三方库链接
find_package(nlohmann_json REQUIRED)
target_link_libraries(fstd PRIVATE
    spdlog
    fmt
    zstd
    # nlohmann_json::nlohmann_json
)

# 覆盖率链接配置
if (CMAKE_BUILD_TYPE_LOWER STREQUAL "coverage")
    target_link_libraries(fstd PRIVATE
        --coverage
        -fprofile-arcs
        -ftest-coverage
    )

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
    target_link_libraries(fstd PRIVATE -lprofiler)
endif()

# 安装
install(TARGETS fstd
    DESTINATION /usr/local/bin
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
)