add_executable(benchmark_main benchmark_main.cpp)
target_link_libraries(benchmark_main PRIVATE ob_core)
target_compile_options(benchmark_main PRIVATE -O3 -Wall -Wextra)
target_compile_definitions(benchmark_main PRIVATE NDEBUG)   # no asserts in measured code