#!/usr/bin/env bash
# PX4 SITL failsafe suite.
# Exit 0 only after the suite actually runs.
# Exit 2 when PX4 SITL is not installed (explicit skip, not a pass).
# Exit 1 when a binary is present but this checkout cannot run the suite.
set -euo pipefail

echo "PX4 SITL failsafe suite (animal climb RTL, low battery, geofence, link loss, Jetson loss)."
echo "PX4 owns these failsafes. This script does not report a pass unless SITL runs."

found=""
if command -v px4 >/dev/null 2>&1; then
  found="$(command -v px4)"
elif [[ -n "${PX4_ROOT:-}" && -x "${PX4_ROOT}/build/px4_sitl_default/bin/px4" ]]; then
  found="${PX4_ROOT}/build/px4_sitl_default/bin/px4"
fi

if [[ -z "$found" ]]; then
  echo "SKIP: PX4 SITL is not installed (no px4 on PATH, and PX4_ROOT has no sitl binary)."
  echo "The failsafe suite did not run. This is not a green SITL result."
  exit 2
fi

echo "FAIL: found ${found}, but this checkout has no Gazebo world or PX4 test harness wired."
echo "Refusing to report a simulated pass."
exit 1
