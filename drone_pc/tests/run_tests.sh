#!/usr/bin/env bash
# Shared contract: the same cases must pass in Python and C++17.
set -euo pipefail
PKG="$(cd "$(dirname "$0")/.." && pwd)"
REPO="$(cd "$PKG/.." && pwd)"
export PYTHONPATH="$REPO${PYTHONPATH:+:$PYTHONPATH}"
python3 -m unittest drone_pc.tests.test_contract
mkdir -p "$PKG/build"
g++ -std=c++17 -Wall -Wextra -O2 -I "$PKG" \
  -o "$PKG/build/drone_pc_contract_tests" \
  "$PKG/tests/test_contract.cpp"
"$PKG/build/drone_pc_contract_tests" "$PKG"
python3 -m py_compile \
  "$PKG/planning/planner_node.py" \
  "$PKG/control/offboard_node.py" \
  "$PKG/safety/safety_node.py" \
  "$PKG/launch/drone_pc.launch.py" \
  "$PKG/launch/px4_sitl.launch.py"
