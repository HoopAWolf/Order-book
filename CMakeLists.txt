cmake_minimum_required(VERSION 3.16)
project(orderbook CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default to Release so benchmarks are optimised
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

find_package(Threads REQUIRED)

# Core library: everything except main
add_library(ob_core INTERFACE)          # header-only for now
target_include_directories(ob_core INTERFACE include)
target_compile_features(ob_core INTERFACE cxx_std_17)
target_link_libraries(ob_core INTERFACE Threads::Threads)

add_executable(orderbook src/main.cpp)
target_link_libraries(orderbook PRIVATE ob_core)
target_compile_options(orderbook PRIVATE -Wall -Wextra -Wpedantic)

include(FetchContent)
FetchContent_Declare(googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_subdirectory(test)
add_subdirectory(benchmark)