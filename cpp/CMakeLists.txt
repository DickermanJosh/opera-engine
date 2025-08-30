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
)

add_library(opera_core STATIC ${CORE_SOURCES})

# Main executable
add_executable(opera-engine src/main.cpp)
target_link_libraries(opera-engine opera_core)

# Debug executables
add_executable(debug_test src/debug_test.cpp)
target_link_libraries(debug_test opera_core)

add_executable(debug_promotion src/debug_promotion.cpp)
target_link_libraries(debug_promotion opera_core)

add_executable(square_debug src/square_debug.cpp)
target_link_libraries(square_debug opera_core)

# Optional: Find and link pybind11 for Python bindings
find_package(pybind11 QUIET)
if(pybind11_FOUND)
    pybind11_add_module(opera_python src/python/bindings.cpp)
    target_link_libraries(opera_python PRIVATE opera_core)
endif()

# Testing
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    find_package(GTest QUIET)
    if(GTest_FOUND)
        enable_testing()
        add_subdirectory(tests)
    else()
        message(WARNING "Google Test not found. Tests will not be built.")
    endif()
endif()

# Install targets
install(TARGETS opera-engine DESTINATION bin)
if(TARGET opera_core)
    install(TARGETS opera_core DESTINATION lib)
endif()