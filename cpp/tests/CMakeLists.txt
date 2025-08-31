# Test configuration for Opera Engine
cmake_minimum_required(VERSION 3.16)

# Find Google Test
find_package(GTest REQUIRED)

# Include directories
include_directories(../include)

# Test sources
set(TEST_SOURCES
    BoardTest.cpp
    MoveGenTest.cpp
    PawnMoveTest.cpp
)

# Create test executable
add_executable(opera_tests ${TEST_SOURCES})

# Link against the core library and Google Test
target_link_libraries(opera_tests 
    opera_core
    GTest::gtest 
    GTest::gtest_main
)

# Add compiler flags for testing
target_compile_options(opera_tests PRIVATE
    -Wall -Wextra -g -O0 --coverage
)

# Link flags for coverage
target_link_options(opera_tests PRIVATE --coverage)

# Register tests with CTest
include(GoogleTest)
gtest_discover_tests(opera_tests)

# Custom target to run tests with coverage
add_custom_target(test_coverage
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/coverage
    COMMAND gcov ${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/opera_tests.dir/*.gcno
    COMMAND lcov --capture --directory ${CMAKE_CURRENT_BINARY_DIR} --output-file ${CMAKE_BINARY_DIR}/coverage/coverage.info
    COMMAND lcov --remove ${CMAKE_BINARY_DIR}/coverage/coverage.info '/usr/*' '*/test/*' --output-file ${CMAKE_BINARY_DIR}/coverage/coverage_filtered.info
    COMMAND genhtml ${CMAKE_BINARY_DIR}/coverage/coverage_filtered.info --output-directory ${CMAKE_BINARY_DIR}/coverage/html
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Generating test coverage report"
    DEPENDS opera_tests
)

# Print coverage summary
add_custom_target(coverage_summary
    COMMAND lcov --summary ${CMAKE_BINARY_DIR}/coverage/coverage_filtered.info
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Printing coverage summary"
    DEPENDS test_coverage
)