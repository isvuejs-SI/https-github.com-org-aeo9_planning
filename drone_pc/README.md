# drone_pc

Planning & Control for an autonomous solar-charging drone (Jetson companion + PX4).

**Hard rule:** PX4 owns final failsafes. This package only plans/supervises and sends **requests** (arm, takeoff, hold, RTL, land, offboard setpoints + heartbeat).

## Layout

```
drone_pc/
  planning/   # state machine, map, global/local/energy/escape planners
  control/    # PX4 request interface, link-loss monitor
  safety/     # preflight gate, ntfy alerts, event logger
  tests/      # shared JSON table + C++/Python runners
  config/     # drone_pc_params.yaml — every limit lives here
  launch/     # PX4 SITL launch
```

Every module has **C++17** (`include/`, `src/`) and **Python** (`drone_pc/`) implementations. Shared cases in `tests/shared/test_cases.json` must produce identical outputs.

## Parameters

All limits are in `config/drone_pc_params.yaml`. No magic numbers in logic modules.

## Tests (no flight required)

```bash
./tests/run_all.sh
```

This runs the shared table in Python and C++, then asserts identical outputs.

## ROS 2 / PX4 SITL

With a ROS 2 workspace and PX4 SITL running:

```bash
colcon build --packages-select drone_pc
source install/setup.bash
ros2 launch drone_pc drone_pc.launch.py
```

Hardware drivers (Livox Mid-360, thermal camera, Starlink, real ntfy POST) are stubbed; core logic is real and covered by the shared tests.

## Spec coverage

| # | Area | Status |
|---|------|--------|
| 1 | Package skeleton, launch, params | Done |
| 2 | State machine | Done (C++/Py) |
| 3 | Preflight gate | Done (C++/Py) |
| 4 | Map / obstacles | Done (logic; Livox/thermal drivers stubbed) |
| 5 | Global A* | Done (C++/Py) |
| 6 | Local planner | Done (C++/Py) |
| 7 | Energy-aware planning | Done (C++/Py) |
| 8 | Escape planner | Done (C++/Py) |
| 9 | PX4 request interface | Done (SITL stub transport) |
| 10 | Link loss | Done (C++/Py) |
| 11 | Alerts / logging | Done (ntfy network stubbed) |
| 12 | Shared dual-language tests | Done |
