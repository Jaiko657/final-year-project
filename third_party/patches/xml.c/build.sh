#!/usr/bin/env bash
set -e

echo "[xml.c] Building library..."

mkdir -p build
cd build

cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release --parallel
ctest --output-on-failure

echo "[xml.c] Build + tests complete."
