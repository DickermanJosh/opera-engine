cmake_minimum_required(VERSION 3.16)
project(OperaEngine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3 -flto")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -march=native")
endif()

if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /O2 /GL")
endif()

# Include directories
include_directories(include)

# Core engine library
set(CORE_SOURCES
    src/board/Board.cpp
    src/board/MoveGenerator.cpp
    src/utils/Types.cpp
    src/search/search_engine.cpp
    src/search/alphabeta.cpp
    src/search/transposition_table.cpp
    src/search/move_ordering.cpp
    src/search/see.cpp
    src/eval/handcrafted_eval.cpp
    src/eval/morphy_eval.cpp
)

add_library(opera_core STATIC ${CORE_SOURCES})

# Main executable
add_executable(opera-engine src/main.cpp)
target_link_libraries(opera-engine opera_core)

# Perft runner executable
add_executable(perft-runner src/perft/PerftRunner.cpp)
target_link_libraries(perft-runner opera_core)

# Debug executables (optional - build manually from debug/ directory as needed)
# All debug files have been moved to debug/ directory
# To build: g++ -std=c++17 -I../include -I../src/board debug/debug_*.cpp ../src/board/*.cpp ../src/utils/*.cpp -o debug_program

# Optional: Find and link pybind11 for Python bindings
find_package(pybind11 QUIET)
if(pybind11_FOUND)
    pybind11_add_module(opera_python src/python/bindings.cpp)
    target_link_libraries(opera_python PRIVATE opera_core)
endif()

# Testing
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    # Try to find system-installed Google Test first
    find_package(GTest QUIET)

    if(NOT GTest_FOUND)
        # If not found, download and build it automatically (cross-platform)
        message(STATUS "Google Test not found on system. Fetching from GitHub...")
        include(FetchContent)

        FetchContent_Declare(
            googletest
            GIT_REPOSITORY https://github.com/google/googletest.git
            GIT_TAG v1.14.0  # Latest stable release
            GIT_SHALLOW TRUE
        )

        # Prevent overriding parent project's compiler/linker settings on Windows
        set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

        # Make available
        FetchContent_MakeAvailable(googletest)

        # Create aliases for consistency
        add_library(GTest::gtest ALIAS gtest)
        add_library(GTest::gtest_main ALIAS gtest_main)

        message(STATUS "Google Test successfully downloaded and configured.")
    else()
        message(STATUS "Using system-installed Google Test.")
    endif()

    enable_testing()
    add_subdirectory(tests)
endif()

# Install targets
install(TARGETS opera-engine DESTINATION bin)
if(TARGET opera_core)
    install(TARGETS opera_core DESTINATION lib)
endif()