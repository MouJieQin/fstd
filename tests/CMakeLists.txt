# Define the test executable
add_executable(run_tests test_fstd.cpp)

# Define compile-time directory definitions
target_compile_definitions(run_tests
    PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/"
)

# Set include directories
target_include_directories(run_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/lib/include
)

# Link testing, local, and external dependencies using clean namespaces
target_link_libraries(run_tests
    PRIVATE
    fstd_static
    GTest::gtest
    GTest::gtest_main
    spdlog::spdlog
    fmt::fmt
    nlohmann_json::nlohmann_json
    indicators::indicators
    CLI11::CLI11
    zstd::zstd        # UPDATED: Changed from variable to uniform namespace
    pcre2::pcre2      # UPDATED: Changed from variable to uniform namespace
)

# Register the unit test runner with CTest
add_test(
    NAME fstd_all_tests
    COMMAND run_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

# Custom target: 'make check' to build and instantly execute with console color output
add_custom_target(check
    COMMAND run_tests
    DEPENDS run_tests
    COMMENT "Building and running tests..."
    USES_TERMINAL
)

# Custom target: 'make ctest' to run via the active standard cmake testing harness
add_custom_target(ctest
    COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
    DEPENDS run_tests
    COMMENT "Running test suite via CTest harness..."
)
