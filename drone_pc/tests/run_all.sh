#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

echo "== Python shared tests =="
python3 tests/python/run_shared_tests.py

echo "== C++ shared tests (standalone) =="
BUILD="$ROOT/build_standalone"
rm -rf "$BUILD"
mkdir -p "$BUILD"
cp CMakeLists.standalone.txt "$BUILD/CMakeLists.txt"
ln -sfn "$ROOT/src" "$BUILD/src"
ln -sfn "$ROOT/include" "$BUILD/include"
ln -sfn "$ROOT/tests" "$BUILD/tests"
CXX="${CXX:-g++}" CC="${CC:-gcc}" cmake -S "$BUILD" -B "$BUILD/out" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD/out" -j"$(nproc)"
"$BUILD/out/drone_pc_cpp_tests" "$ROOT/tests/shared"

echo "== Compare C++ vs Python outputs =="
python3 tests/compare_results.py

echo "All shared dual-language tests OK."
