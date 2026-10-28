# AEO-9 Planning & Control (simulation-first)

**Goal:** a mission supervisor for the AEO-9 hexacopter that plans legs/days, tracks energy, and decides HOLD / RTL / LAND. PX4 keeps its own failsafes; the supervisor only requests mode changes or waypoints. Every part has matching Python 3 and C++17 twins that must print identical output.

## Safety / legal
- Drones are banned in Grand Canyon National Park without written authorization (36 CFR 1.5). Planned routes and bases stay **outside the park**; FAA Part 107 and Remote ID still apply.
- All numbers are **illustrative placeholders**, not real AEO-9 specs.
- **Software-only** until hardware starts in November. No real flight code is enabled.

## Layout
```
failsafe/     drone failsafe gate sim (failsafe_sim.py/.cpp, drone_failsafe_gate.yaml, golden.txt, run_ci.sh)
mission_l17/  Lesson 17 five-day mission sim (aeo9_mission.*)
day3_l18/     Lesson 18 Day 3 filming-day sim (aeo9_day3.*, writes budget CSVs)
docs/lessons/ lesson write-ups (failsafe guide, L17, L18)
run_ci.sh     runs all three parts' CI
CURSOR_TASKS.md  ordered build tasks for Cursor
.cursorrules  project rules for Cursor
```

## Requirements
python3, g++ with C++17, bash.

## Run each part
```
python3 failsafe/failsafe_sim.py
python3 mission_l17/aeo9_mission.py
python3 day3_l18/aeo9_day3.py out.csv
```
CI per part: `bash failsafe/run_ci.sh`, `bash mission_l17/run_ci.sh`, `bash day3_l18/run_ci.sh` (each builds the C++ twin, diffs it against Python and the golden file).

## Run all CI
```
./run_ci.sh
```
Expect `ALL CI OK`.
