#!/usr/bin/env bash
# Host unit tests for the DevOps packages. Does not start ROS 2, PX4, or CUDA.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"
# The c++ alternative on some images is clang without a libstdc++ link. g++ is the host compiler.
export CC="${CC:-gcc}"
export CXX="${CXX:-g++}"
cmake -S "$ROOT" -B "$ROOT/build" -DCMAKE_BUILD_TYPE=Release
cmake --build "$ROOT/build" -j"$(nproc)"
ctest --test-dir "$ROOT/build" --output-on-failure
export DRONE_OTA_CLI="$ROOT/build/drone_ota/drone_ota_cli"
export PYTHONPATH="$ROOT${PYTHONPATH:+:$PYTHONPATH}"
python3 -m pytest -q
