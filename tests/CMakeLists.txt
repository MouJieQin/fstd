add_executable(run_tests test_fstd.cpp)

target_compile_definitions(run_tests
    PRIVATE
    TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/"
)

target_include_directories(run_tests PRIVATE
    ${PROJECT_SOURCE_DIR}/lib/include
)

target_link_libraries(run_tests
    PRIVATE
    fstd_static
    GTest::gmock
    GTest::gmock_main
    spdlog
    fmt
    zstd
    pcre2-8
    nlohmann_json::nlohmann_json
)

add_test(
    NAME fstd_all_tests
    COMMAND run_tests
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)

add_custom_target(check
    COMMAND ${CMAKE_CURRENT_BINARY_DIR}/run_tests
    DEPENDS run_tests
    COMMENT "Build test code and run tests"
    USES_TERMINAL  # To ensure color output in the terminal
)

add_custom_target(ctest
    COMMAND ${CMAKE_CTEST_COMMAND}
    DEPENDS run_tests
    COMMENT "Build test code and run tests"
)