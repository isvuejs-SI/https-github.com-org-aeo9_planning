#!/usr/bin/env bash
# AEO-9 CI: Python pytest + C++ GoogleTest + golden-file twin diff.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

export PYTHONPATH="$ROOT${PYTHONPATH:+:$PYTHONPATH}"
export PATH="${HOME}/.local/bin:${PATH}"

echo "==> Python unit/scenario tests"
python3 -m pytest -q tests/python

echo "==> Configure & build C++"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"

echo "==> C++ GoogleTest"
ctest --test-dir build --output-on-failure

echo "==> Golden twin dump (Python vs C++)"
GOLDEN="$ROOT/tests/golden/supervisor_dump.txt"
PY_OUT="$(mktemp)"
CPP_OUT="$(mktemp)"
trap 'rm -f "$PY_OUT" "$CPP_OUT"' EXIT

python3 -m planner.dump --config "$ROOT/config/aeo9.yaml" >"$PY_OUT"
"$ROOT/build/aeo9_dump" "$ROOT/config/aeo9.yaml" >"$CPP_OUT"

if ! diff -u "$GOLDEN" "$PY_OUT"; then
  echo "FAIL: Python dump differs from golden" >&2
  exit 1
fi
if ! diff -u "$PY_OUT" "$CPP_OUT"; then
  echo "FAIL: C++ dump differs from Python" >&2
  exit 1
fi

echo "==> CI green"
