#!/usr/bin/env bash
set -e

echo "[raylib] Building library..."

# Build a release static library without the example suite to keep the build fast
# and avoid optional dependencies pulled in by the samples.
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_EXAMPLES=OFF \
  -DBUILD_SHARED_LIBS=OFF

cmake --build build --config Release --target raylib --parallel

echo "[raylib] Build complete."
