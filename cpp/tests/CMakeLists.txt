# Test configuration for Opera Engine
cmake_minimum_required(VERSION 3.16)

# Google Test is already found/fetched by parent CMakeLists.txt
# No need to find it again here

# Include directories
include_directories(../include)

# Test sources
set(TEST_SOURCES
    AttackingPrinciplesTest.cpp
    CoreRegressionTest.cpp
    BoardTest.cpp
    MoveGenTest.cpp
    PawnMoveTest.cpp
    KnightMoveTest.cpp
    SlidingPieceTest.cpp
    KingMoveTest.cpp
    ChessRulesTest.cpp
    PerformanceTest.cpp
    BoardPerformanceTest.cpp
    MemoryAuditTest.cpp
    SearchEngineTest.cpp
    TranspositionTableTest.cpp
    MoveOrderingTest.cpp
    StaticExchangeTest.cpp
    AlphaBetaTest.cpp
    SearchOptimizationTest.cpp
    UCIOptionsTest.cpp
    SearchControlTest.cpp
    EvaluatorInterfaceTest.cpp
    HandcraftedEvalTest.cpp
    AdvancedEvalTest.cpp
    MorphyEvalTest.cpp
    EvalCachingTest.cpp
    SearchEvalIntegrationTest.cpp
    PerformanceBenchmarkTest.cpp
    TacticalEPDTest.cpp
    MorphyStyleValidationTest.cpp
)

# Create test executable
add_executable(opera_tests ${TEST_SOURCES})

# Link against the core library and Google Test
target_link_libraries(opera_tests 
    opera_core
    GTest::gtest 
    GTest::gtest_main
)

# Keep the selected build configuration. Instrumentation is opt-in so Release
# benchmarks measure Release code, and MSVC never receives GNU coverage flags.
option(OPERA_COVERAGE "Instrument the core and tests for gcov coverage" OFF)
if(OPERA_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT MSVC)
    target_compile_options(opera_core PRIVATE -O0 -g --coverage)
    target_link_options(opera_core INTERFACE --coverage)
    target_compile_options(opera_tests PRIVATE -O0 -g --coverage)
endif()

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
