#!/bin/bash
cmake -S . -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug -j
ctest --test-dir build-debug --output-on-failure