# drone_safety_ops

Simulation-only safety supervisor for a Jetson companion computer. PX4 on the Pixhawk owns the final failsafes. The Jetson only supervises and sends requests.

Rule: PX4 on the Pixhawk owns the final failsafes. The Jetson only supervises and sends REQUESTS.

Requests that may be sent are hold, RTL, land, and no-arm. `CLIMB_ESCAPE` and `ESCAPE` are published and logged. This package never sends an arm. `PROP_LOCKOUT` and `NO_GO` stay in the rules.

| Module | What it does | Verdicts (worse wins) |
|---|---|---|
| supervisor | In-flight supervisor | CONTINUE < HOLD < CLIMB_ESCAPE < RTL < LAND < NO_TAKEOFF |
| mission_gate | Return reserve, overnight solar test, link-loss ladder (over 30 s RTL, over 600 s abort), MAX_DAYS = 5 | GO < HOLD_GROUND < RTL < ABORT_MISSION < LAND_NOW < NO_GO |
| day3 | Multi-day simulation using the gate. Placeholder numbers, SITL only | same as mission_gate |
| guard | Ground guard when parked or charging: LiDAR + thermal | STAY, WATCH, DETER, ESCAPE, PROP_LOCKOUT |
| alert | ntfy push. Set `NTFY_TOPIC` to a long random name. 60 s duplicate hold-back | |
| landing | Post-landing checks and next-leg go/no-go. Voted only while not airborne | GO, HOLD_GROUND, NO_GO |
| route | Multi-leg route feasibility | GO_SIM, NO_GO |

Guard rules: person within 5 m = PROP_LOCKOUT (never arm); over 15 m = WATCH; animal within 8 m and closing, battery >= 30%, cover open, wind OK = ESCAPE, otherwise DETER.

Those distances and the other thresholds live in `config/params.yaml`. Battery endurance 14 min and leg 44.5 km are relayed. Cruise 40 km/h is assumed. They stay labeled `relayed` or `assumed` until they are replaced with measured values. Do not treat them as flight data.

Real flight stays `NO_GO` unless tribal permission and a Part 107 waiver are both set, the park is clear, and the relayed leg fits in one pack. With the example numbers the leg does not fit, so a real-flight plan is `NO_GO`. `GO_SIM` is the simulation result only. Setting `route.real_flight` true does not authorize a flight.

## Legal and operating facts (cite before real flights)

- Grand Canyon NP bans launching, landing, operating drones without written superintendent approval (2026 Superintendent's Compendium, 36 CFR 1.5).
- Havasupai Reservation bans drones without written advance permission from the Tribal Chairperson or Council (Tribe Code Chapter 18.8).
- Beyond visual line of sight under Part 107 needs a waiver of 107.31 (FAA DroneZone). Remote ID required (Part 89). Part 108 is proposed, not in effect.
- Simulation first (PX4 SITL). No real flights until every permission is in hand.

## Topics

Each verdict node subscribes to:

- `/drone_safety_ops/battery` JSON: `soc_pct`, `cell_temp_c`, `pack_wh`, `airborne`, `armed`, `cover_open`, `wind_ms`, `rain`, `wind_ok`, `props_ok`
- `/drone_safety_ops/gps_geofence` JSON: `gps_ok`, `inside_fence`, `alt_m`, `dist_home_km`, `legal_area_ok`, `geofence_ok`
- `/drone_safety_ops/link_heartbeat` JSON: `remote_id_ok`, `observer_ok`, `phase`, `day`, `solar_forecast_wh`, optional `link_gap_s`. The node also measures time since the last message.
- `/drone_safety_ops/lidar` JSON: `kind` (`none`, `human`, `animal`), `dist_m`, `closing_mps`, optional `intrusion`
- `/drone_safety_ops/thermal` JSON: same detection fields, optional `cell_temp_c`. A fresh thermal message marks the detector healthy. Silence longer than `detector_timeout_s` is a detector timeout.

The safety supervisor publishes `/drone_safety_ops/worst_verdict` and the per-module verdict topics. The PX4 request is JSON on `/drone_safety_ops/px4_vehicle_command` (see `px4.request_topic`). The `uxrce_topic` field is `/fmu/in/vehicle_command`. Copy `command`, `param1`, `param2`, and `param3` into the PX4 `VehicleCommand` for uXRCE-DDS. Command ids: hold = 176 with PX4 AUTO.LOITER (param1 = 1, param2 = 4, param3 = 3), RTL = 20, land = 21, no-arm = 400 with param1 = 0. No-arm is sent on the ground. In the air, a no-arm verdict is sent as land and the no-arm latch is published on `/drone_safety_ops/no_arm_latch`.

Set `px4.transport` to `mavsdk` and `MAVSDK_CONNECTION` to also call `action.hold`, `action.return_to_launch`, `action.land`, or `action.disarm` from the Python node. The C++ node publishes the same request and the matching `mavsdk` action name for uXRCE-DDS. Neither path overrides the Pixhawk failsafes.

## Watchdog, log, alert

`systemd/drone-safety-ops.service` and `systemd/drone-safety-ops-cpp.service` use `Type=notify` and `WatchdogSec=5`. The node sends `READY=1` and `WATCHDOG=1` on `NOTIFY_SOCKET` and publishes `/drone_safety_ops/heartbeat`.

Every verdict and each transition is appended to `verdicts.jsonl` under `logging.nvme_dir` (`/mnt/nvme/drone_safety_ops`). If that directory is not writable the log falls back to `logging.fallback_dir`.

Alerts use `NTFY_TOPIC`. The same verdict is held back for 60 s. `STAY` does not alert. If `NTFY_TOPIC` is unset the text is logged and nothing is sent.

## Build and test

Shared C++ and Python verdict tests need g++ and Python 3. They do not need ROS 2 or PX4:

```bash
make -C drone_safety_ops test
```

On a machine with ROS 2 Jazzy or Humble:

```bash
colcon build --packages-select drone_safety_ops
ros2 launch drone_safety_ops safety_ops.launch.py
ros2 launch drone_safety_ops safety_ops_py.launch.py
```

`test/launch/test_safety_ops_launch.py` is a `launch_testing` test. `test/sitl/test_px4_sitl.py` exits 77 until a PX4 SITL run actually completes. Neither result is a pass when the tool is absent.

Day 3 rows are placeholder simulation numbers. The legal area in that printout is outside Grand Canyon National Park, with visual line of sight and an observer. That sentence is a label on the simulation, not permission to fly.
