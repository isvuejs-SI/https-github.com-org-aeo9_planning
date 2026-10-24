# AEO-9 Planning & Control Supervisor

Software-only, simulation-first mission supervisor for the AEO-9 drone.

The supervisor **plans the mission** and decides **hold / return / land**. PX4 keeps its own failsafes; this stack only asks PX4 for a mode change or a waypoint.

## Status

- **Sim only** until hardware (November). Real MAVSDK flight path is disabled.
- Twin **Python 3** and **C++17** implementations must print identical golden output.
- Planned staging home (`tusayan_staging`) stays **outside Grand Canyon National Park** (36 CFR 1.5 — drones banned in GCNP without authorization).

## Layout

| Path | Role |
|------|------|
| `planner/` | Config, energy model, mission plan, go/no-go |
| `control/` | Failsafe, geofence, animal escape, low battery, gear |
| `sim/` | PX4 adapter mock, ROS 2 stubs, CSV logger, Isaac stubs |
| `cpp/` | C++17 twin library + `aeo9_dump` |
| `config/aeo9.yaml` | Integer-unit flight limits |
| `tests/` | pytest, GoogleTest, golden file, scenarios |
| `run_ci.sh` | Build + test + golden twin diff |
| `logs/` | CSV run logs (gitignored) |

## Quick start

```bash
sudo apt-get install -y g++ cmake libgtest-dev libgmock-dev libyaml-cpp-dev libstdc++-14-dev
python3 -m pip install -r requirements.txt
./run_ci.sh
```

Deterministic dump (must match across languages):

```bash
PYTHONPATH=. python3 -m planner.dump --config config/aeo9.yaml
./build/aeo9_dump config/aeo9.yaml
```

## Stack

- PX4 SITL + Pegasus Simulator in Isaac Sim (stubs until wired)
- MAVSDK / MAVLink behind `sim.px4_adapter` (mock in tests)
- ROS 2 Jazzy topics stubbed in `sim.ros2_nodes`
- pytest + GoogleTest + golden-file diff in CI

## Rules

- Deterministic: fixed seed; integer math (cm, mWh, e7 lat/lon)
- No keys, footage, or run logs in Git
- Each feature ships with a test before it counts as done
