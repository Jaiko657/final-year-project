#!/usr/bin/env bash
set -e

echo "[Chipmunk2D] Building library..."

cmake -S . -B build
cmake --build build --config Release --parallel

echo "[Chipmunk2D] Build complete."