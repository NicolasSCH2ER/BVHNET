cmake_minimum_required(VERSION 3.20)
project(renderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Release by default
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(renderer
    src/main.cpp
)

target_include_directories(renderer PRIVATE src third_party)

# Compiler-specific optimisation flags
if(MSVC)
    target_compile_options(renderer PRIVATE /O2 /arch:AVX2 /W4)
else()
    target_compile_options(renderer PRIVATE -O3 -march=native -Wall -Wextra)
endif()

# OpenMP (optional — enables multi-threaded rendering when available)
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(renderer PRIVATE OpenMP::OpenMP_CXX)
    message(STATUS "OpenMP found — parallel rendering enabled")
else()
    message(STATUS "OpenMP not found — single-threaded rendering")
endif()
