#!/usr/bin/env python3
"""PX4 SITL hook. Exits 77 unless a SITL run actually completes.

When PX4 SITL and ROS 2 are both installed, a future run of this test should:
- start px4 in SITL only
- launch drone_safety_ops
- publish an airborne low-battery sample
- assert the bridge emits RTL (MAVLink command 20) and does not send an arm

This script does not start a real vehicle and does not waive Grand Canyon,
Havasupai, Part 107, or Remote ID requirements.
"""

import os
import shutil
import sys


def main():
    px4 = shutil.which("px4")
    root = os.environ.get("PX4_ROOT", "")
    root_ok = bool(root) and os.path.isdir(root)
    if not px4 and not root_ok:
        print("SKIP PX4 SITL: px4 binary and PX4_ROOT are absent. This is not a pass.")
        return 77
    print("PX4 SITL binary is present, but this environment did not execute a SITL flight. This is not a pass.")
    return 77


if __name__ == "__main__":
    sys.exit(main())
