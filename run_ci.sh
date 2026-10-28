#!/usr/bin/env bash
set -uo pipefail
cd "$(dirname "$0")"
fail=0
for p in failsafe mission_l17 day3_l18 tests sim; do
  echo "== $p =="
  if bash "$p/run_ci.sh"; then echo "$p: PASS"; else echo "$p: FAIL"; fail=1; fi
done
[ $fail -eq 0 ] && echo "ALL CI OK" || { echo "SOME CI FAILED"; exit 1; }
