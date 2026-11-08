#!/bin/bash
set -e   # stop if the build or the benchmark fails

cmake -S . -B build-benchmark -DCMAKE_BUILD_TYPE=Release
cmake --build build-benchmark -j
./build-benchmark/benchmark/benchmark_main 1000000 42 2 build-benchmark/benchmark/base_benchmark.csv  # messages, seed, core, save to file

.venv/bin/python benchmark/analyze.py \
    build-benchmark/benchmark/base_benchmark.csv \
    build-benchmark/benchmark/base_benchmark.png