# 1. FIXED: Changed target name from 'run_tests' to 'fstd_test_runner'
add_executable(fstd_test_runner test_fstd.cpp)

# Define compile-time directory definitions
target_compile_definitions(fstd_test_runner
    PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/"
)

# Set include directories
target_include_directories(fstd_test_runner PRIVATE
    ${PROJECT_SOURCE_DIR}/lib/include
)

# Link testing, local, and external dependencies using clean namespaces
target_link_libraries(fstd_test_runner
    PRIVATE
    fstd_static
    GTest::gtest
    GTest::gtest_main
    spdlog::spdlog
    fmt::fmt
    nlohmann_json::nlohmann_json
    indicators::indicators
    CLI11::CLI11
    zstd::zstd        
    pcre2::pcre2      
)

# Register the unit test runner with CTest
add_test(
    NAME fstd_all_tests
    # 2. FIXED: Reference the updated binary name
    COMMAND fstd_test_runner
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Custom target: 'make check' to build and instantly execute with console color output
add_custom_target(check
    COMMAND fstd_test_runner
    DEPENDS fstd_test_runner
    COMMENT "Building and running tests..."
    USES_TERMINAL
)

# Custom target: 'make ctest' to run via the active standard cmake testing harness
add_custom_target(ctest_run
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS fstd_test_runner
    COMMENT "Running test suite via CTest harness..."
)
