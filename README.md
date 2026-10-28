# AEO-9 Planning & Control (simulation-first)

**Goal:** a mission supervisor for the AEO-9 hexacopter that plans legs/days, tracks energy, and decides HOLD / RTL / LAND. PX4 keeps its own failsafes; the supervisor only requests mode changes or waypoints. Every part has matching Python 3 and C++17 twins that must print identical output.

## Safety / legal
- Drones are banned in Grand Canyon National Park without written authorization (36 CFR 1.5). Planned routes and bases stay **outside the park**; FAA Part 107 and Remote ID still apply.
- All numbers are **illustrative placeholders**, not real AEO-9 specs.
- **Software-only** until hardware starts in November. No real flight code is enabled.

## Layout
```
config/aeo9.yaml   shared placeholders (wind, reserve, fence, min AGL, route outside the park)
planner/           YAML loader, energy step, leg/day planner (Python + C++17 headers)
control/           go/no-go, latched failsafe, geofence, escape, low-battery return,
                   gear/solar sequence, PX4 mock, ROS 2 mock
sim/               seven-scenario sortie runner and per-minute CSV
tests/             one printed check per behavior; Python/C++ diff
failsafe/          reference failsafe gate sim (unchanged)
mission_l17/       reference five-day mission sim (unchanged)
day3_l18/          reference Day 3 filming sim (unchanged)
docs/lessons/      lesson write-ups
run_ci.sh          runs the reference sims plus tests/ and sim/
CURSOR_TASKS.md    ordered build tasks
.cursorrules       project rules
```

## Requirements
python3, g++ with C++17, bash.

## Run each part
```
python3 failsafe/failsafe_sim.py
python3 mission_l17/aeo9_mission.py
python3 day3_l18/aeo9_day3.py out.csv
python3 tests/stack_test.py
python3 sim/run_stack.py sim/budget.csv
```
CI per part: `bash failsafe/run_ci.sh`, `bash mission_l17/run_ci.sh`, `bash day3_l18/run_ci.sh`, `bash tests/run_ci.sh`, `bash sim/run_ci.sh`. Each builds the C++ twin and diffs it against Python. Mission, Day 3, `tests/`, and `sim/` also diff a golden file. `sim/` compares the per-minute CSV as well.

The supervisor only requests arm, takeoff, goto, hold, return, or land. The PX4 and ROS 2 pieces are in-process mocks: they do not open a socket, link MAVSDK, or start a ROS graph. PX4 keeps its own failsafes.

## Run all CI
```
./run_ci.sh
```
Expect `ALL CI OK`.
