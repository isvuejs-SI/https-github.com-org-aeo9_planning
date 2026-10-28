# drone_pc

Planning and control supervisor for a solar-charging multirotor. The Jetson plans and supervises. The Pixhawk running PX4 owns motors, actuators, and failsafes. This package never arms ESCs, never publishes actuator outputs, and never writes PX4 failsafe parameters. It sends requests that PX4 may accept or reject.

## Hardware

Documented interfaces only. Vendor drivers are not included, because they cannot be tested in this tree.

| Item | Role in this package |
| --- | --- |
| Jetson Orin | Host for this supervisor. |
| Pixhawk (PX4) | Flight controller. Final failsafes stay here. |
| Livox Mid-360 | `sensor_msgs/PointCloud2` on `topics.pointcloud` (default `/livox/lidar`). No Livox SDK. |
| Thermal camera | JSON detections on `topics.thermal` (default `/thermal/detections`). People and animals become moving obstacles. No camera driver. |
| Sony ZV-E10 II | Payload camera. Not used by planning or control. |
| Starlink Mini | Link-up signal on `topics.link` (default `/link/up`). No Starlink API client. |

Thermal messages look like:

```json
{"detections":[{"class":"person","x":1.0,"y":2.0,"z":0.5,"vx":0.0,"vy":0.0,"vz":0.0,"radius_m":0.5}]}
```

Classes other than `person` and `animal` are ignored.

## Layout

- `planning/` occupancy grid, A* global planner, local planner, energy, escape
- `control/` mission state machine and the PX4 request interface
- `safety/` preflight gate, link loss, ntfy payload builder, local event log
- `tests/` one JSON case table and the Python and C++ runners
- `config/limits.yaml` every wind, temperature, state-of-charge, geofence, altitude, speed, VLOS, heartbeat, link-loss, return-reserve, escape, and ntfy limit
- `launch/` supervisor launch and the PX4 SITL path

Each module exists in C++17 (headers) and Python. Both runners load `tests/contract_cases.json` and require the same outputs.

## PX4 owns the failsafes

`/drone_pc/px4_request` carries JSON requests. Allowed kinds are `ARM`, `TAKEOFF`, `HOLD`, `RTL`, `LAND`, and `OFFBOARD_SETPOINT`. A setpoint is position only (`x_mm`, `y_mm`, `z_mm`). `ARM` is a vehicle-command request. PX4 still runs its own preflight checks and can refuse it. There is no disarm-by-actuator path and no thrust field.

Offboard setpoints are published on a steady heartbeat (`heartbeat.rate_hz` in the YAML). If this process misses that heartbeat it records the miss and does nothing else. PX4 is expected to fall back to its own Hold or RTL. Leave these parameters under PX4 control:

- `COM_OF_LOSS_T` greater than 0, so PX4 times out a lost offboard stream itself
- the offboard-loss action (Hold or RTL) left at the vehicle's existing setting
- `NAV_DLL_ACT` left at Hold or Return for a datalink loss
- `COM_LOW_BAT_ACT` left enabled

Do not set `CBRK_*` circuit breakers. Do not set a parameter that disables a failsafe. This package has no parameter-write call.

When a PX4 ROS 2 bridge is present, map the requests as follows and still do not publish `ActuatorMotors` or actuator outputs:

| Request | PX4 command |
| --- | --- |
| `ARM` | `VEHICLE_CMD_COMPONENT_ARM_DISARM` with param1 = 1 |
| `TAKEOFF` | `VEHICLE_CMD_NAV_TAKEOFF` |
| `HOLD` | loiter / hold command |
| `RTL` | `VEHICLE_CMD_NAV_RETURN_TO_LAUNCH` |
| `LAND` | `VEHICLE_CMD_NAV_LAND` |
| `OFFBOARD_SETPOINT` | position `TrajectorySetpoint` only |

## Behavior

Mission states: `SLEEP_CHARGING`, `PREFLIGHT_GATE`, `TAKEOFF`, `CRUISE`, `SURVEY`, `RETURN`, `LAND`, `ESCAPE`. Every `handle()` call appends a transition line with a reason, including a self-transition.

Preflight verdicts, worst wins:

`GO < HOLD_GROUND < RTL < ABORT_MISSION < LAND_NOW < NO_GO`

Checks: wind, cell temperature, state of charge, geofence (including no-fly polygons), Remote ID, return reserve, and the operator abort switch.

Return reserve, with factors read from YAML:

`distance_km * Wh_per_km * return_distance_factor + pack_reserve_fraction * pack_Wh`

The overnight choice is `STAY_CHARGE`, `RETURN_NOW`, or `CONTINUE` (airborne only). A backup landing site is the nearest in-fence, out-of-no-fly candidate. If none of those are inside the reserve, the nearest legal site is still returned and marked `energy_shortfall`.

Escape verdicts, worst wins: `STAY < WATCH < DETER < ESCAPE < PROP_LOCKOUT`. A climb toward the backup site is requested only for `ESCAPE` when state of charge is at least `battery.escape_min_soc`, the cover is open, and the wind is at or below `escape.max_wind_mps`. `PROP_LOCKOUT` does not request arm or takeoff.

Link loss uses `link.rtl_after_s` and `link.abort_after_s` (defaults 30 s and 600 s). The timer must be strictly over the limit. Over the RTL limit the supervisor requests RTL. Over the abort limit it aborts the mission and requests land. PX4's own datalink failsafe still applies.

ntfy payloads use `ntfy.base_url` and `ntfy.topic`. Duplicate alerts for the same key are held back for `ntfy.cooldown_s` (default 60 s). `ntfy.enable_send` defaults to false. The sample URL has no token, and the sender does not add an `Authorization` header. Verdicts and transitions are also written under `logging.directory`.

## Numeric contract

C++ and Python share one integer contract so the case table can demand identical outputs:

- positions in millimeters, times in milliseconds, energy in micro-watt-hours
- half-away-from-zero rounding
- floor division toward negative infinity
- A* is 6-connected, step cost is the cell resolution, tie-break is `f`, then `h`, then `z`, `y`, `x`
- neighbor order is `+x, -x, +y, -y, +z, -z`

Geofence and no-fly polygons are hard constraints on the global planner. The local planner clamps altitude, limits speed near occupied cells, sidesteps a blocked leg, and pulls a setpoint back inside the visual-line-of-sight radius.

## Tests

No hardware and no ROS graph are required.

```bash
./drone_pc/tests/run_tests.sh
```

Or, from the repository root:

```bash
export PYTHONPATH="$PWD:$PYTHONPATH"
python3 -m unittest drone_pc.tests.test_contract
g++ -std=c++17 -Wall -Wextra -O2 -I drone_pc \
  -o drone_pc/build/drone_pc_contract_tests drone_pc/tests/test_contract.cpp
./drone_pc/build/drone_pc_contract_tests drone_pc
```

`cmake -S drone_pc -B drone_pc/build -DCMAKE_CXX_COMPILER=g++ && cmake --build drone_pc/build && ctest --test-dir drone_pc/build` builds the same C++ runner. Point CMake at `g++` when the default `c++` driver is not a working GCC.

## PX4 SITL

PX4 SITL is not installed in the environment where this package was written, and this repository does not fly a vehicle.

```bash
cd PX4-Autopilot
make px4_sitl gazebo-classic
```

In another shell, with ROS 2 sourced and this package on `PYTHONPATH` (or installed):

```bash
ros2 launch drone_pc px4_sitl.launch.py
```

The launch file starts the supervisor nodes, prints the failsafe-parameter reminder, and does not change PX4 parameters. If `PX4_SITL_BIN` points at an existing `px4` binary it also starts that process. If the binary is absent it logs the manual command and does not start a flight.

Without ROS 2 the node entry points exit 0 and say that `rclpy` is missing. The contract tests do not need it.

## Limits

Replace the example geofence and no-fly square in `config/limits.yaml` before any real operation. Battery watt-hours, Wh/km, and the wind and temperature ladders are operator parameters, not a claim about a specific pack or airframe. Point `DRONE_PC_LIMITS` at another YAML file to override the default path.
