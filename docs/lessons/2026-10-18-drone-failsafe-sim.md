# Isaac Sim: testing the drone's flight and failsafe logic before you fly
*One-off guide for course 7/8 (Isaac Sim), 2026-10-18. Not a lesson; the weekday schedule stays paused.*
Code: `2026-10-18-drone-failsafe-sim/code/` (`failsafe_sim.py`, `failsafe_sim.cpp`, `drone_failsafe_gate.yaml`, `golden.txt`, `run_ci.sh`).

## 1. Which simulator path is real
**Primary path: Isaac Sim + Pegasus Simulator + PX4 SITL.** [Pegasus Simulator](https://github.com/PegasusSimulator/PegasusSimulator) ([docs](https://pegasussimulator.github.io/PegasusSimulator/)) is an Isaac Sim framework for multirotors. Its [PX4 integration](https://pegasussimulator.github.io/PegasusSimulator/source/features/px4_integration.html) talks MAVLink to PX4 running in SITL, with lockstep available, and can auto-launch PX4. Its [API](https://pegasussimulator.github.io/PegasusSimulator/source/api/index.html) lists Barometer, GPS, IMU, Magnetometer, Lidar and Monocular Camera classes. Pegasus v5.1.0 targets Isaac Sim 5.1.0, and the README badges show PX4 1.14.3 and Ubuntu 22.04. Pin those versions together; the newest PX4 may not match. The ArduPilot interface is [experimental](https://pegasussimulator.github.io/PegasusSimulator/source/features/ardupilot.html) and untested in v5.1.0.

**Alternative: PX4 SITL with Gazebo.** [PX4 Gazebo](https://docs.px4.io/main/en/sim_gazebo_gz/) is the project's mainstream simulator ([simulation overview](https://docs.px4.io/main/en/simulation/)). It is lighter than Isaac Sim and good for pure flight-logic and failsafe tests. Use Isaac Sim when you need RTX sensors or synthetic images. [ArduPilot SITL](https://ardupilot.org/dev/docs/sitl-simulator-software-in-the-loop.html) is the equivalent if you ever pick ArduPilot instead of PX4.

**Talking to the autopilot.** [MAVSDK](https://mavsdk.mavlink.io/main/en/index.html) ([C++](https://mavsdk.mavlink.io/main/en/cpp/guide/), [Python](https://mavsdk.mavlink.io/main/en/python/)) is a good fit for scripted tests. [MAVROS](https://github.com/mavlink/mavros) is the ROS bridge (PX4's [MAVROS install guide](https://docs.px4.io/main/en/ros/mavros_installation.html) is marked ROS 1 / deprecated). PX4's native [ROS 2 interface](https://docs.px4.io/main/en/ros2/user_guide.html) is an alternative, so check which one your Jetson stack really uses. PX4 also documents [failsafe simulation](https://docs.px4.io/main/en/config/safety_simulation.html) and [MAVSDK integration tests](https://docs.px4.io/main/en/test_and_ci/integration_testing_mavsdk.html).

**Isaac Sim side.** Use the [ROS 2 tutorials](https://docs.isaacsim.omniverse.nvidia.com/latest/ros2_tutorials/index.html), including the [RTX Lidar](https://docs.isaacsim.omniverse.nvidia.com/latest/ros2_tutorials/tutorial_ros2_rtx_lidar.html) and [camera](https://docs.isaacsim.omniverse.nvidia.com/latest/ros2_tutorials/tutorial_ros2_camera.html) publishers. Sensor docs: [RTX Lidar](https://docs.isaacsim.omniverse.nvidia.com/latest/sensors/isaacsim_sensors_rtx_lidar.html), [RTX sensors](https://docs.isaacsim.omniverse.nvidia.com/latest/sensors/isaacsim_sensors_rtx.html), [IMU](https://docs.isaacsim.omniverse.nvidia.com/latest/sensors/isaacsim_sensors_physics_imu.html). Synthetic data: [Replicator](https://docs.omniverse.nvidia.com/extensions/latest/ext_replicator.html) and the [Replicator tutorials](https://docs.isaacsim.omniverse.nvidia.com/latest/replicator_tutorials/index.html).

**Honest gaps.**
- **Livox Mid-360:** an RTX Lidar approximates the range, field of view and noise. Livox's non-repeating scan pattern is not something I verified as a ready-made asset, so treat it as a custom config and don't claim an exact match.
- **Thermal camera:** Isaac Sim 5.1 documents no thermal camera sensor. A GitHub reply from NVIDIA says Isaac Sim 6.0 adds a Sensor Processing Graph that can be used to build one ([discussion](https://github.com/isaac-sim/IsaacSim/discussions/298)). Today, a grayscale "pseudo-thermal" render only tests plumbing and detector robustness, not real LWIR behavior.
- **Sony ZV-E10 II control** (gphoto2, Sony SDK, PTP) can't be simulated. Mock a trigger topic and bench-test the real camera.
- **GPS in Isaac Sim:** Pegasus provides its own GPS, barometer and magnetometer models to PX4. Isaac Sim 5.1's own sensor list has no GPS.
- **Starlink, solar charging, BMS, cover/servo and gear hardware** are mocked topics only.
- **Animal behavior** in Isaac Sim is scripted motion. It can test your detector and reaction, not real wildlife.

## 2. Test matrix: your hardware rows
- **Pixhawk:** simulated with PX4 SITL via Pegasus or Gazebo. Bench-test the real flight controller, calibration, sensor orientation, failsafe parameters and arming checks.
- **Gimbal (Gremsy/Pixy):** mock a MAVLink gimbal command listener in sim to check the tracking command path. Bench-test the real gimbal driver, limits and balance.
- **Sony ZV-E10 II:** mock the trigger (a topic that logs "photo/video start"). Bench-test everything real: USB/PTP control, exposure, focus, recording limits.
- **Livox Mid-360:** simulate with an RTX Lidar for the obstacle-detection logic and ROS 2 topic flow. Bench-test the Livox SDK, driver, real scan pattern, vibration and sunlight.
- **Thermal camera:** mock or approximate (see gaps). Bench-test the driver, frame rate, calibration and night detection.
- **YOLO/TensorRT animal detection:** simulate frames with Replicator (RGB, night lighting, animal assets) and replay them through the real TensorRT engine on a Jetson. Bench-test the real thermal and night images, latency and thermals of the Jetson.
- **Starlink Mini upload:** mock a network link with up/down/slow states. Bench-test real throughput, power draw and mounting.
- **Solar/MPPT/BMS:** mock status topics (voltage, current, state of charge). Bench-test the real MPPT, BMS cutoffs and charging in sun.
- **Landing gear, cover, solar deploy:** mock GPIO/servo command topics and check the order and altitude triggers. Bench-test real servos, current draw, jams, wind load and endstops.
- **GPS/altitude logic:** simulate with PX4 SITL and fault injection. Bench-test real GNSS lock, antenna placement and baro drift.
- **NVMe recording:** mock a disk-full condition. Bench-test real write speed and thermal throttling.
- **Remote ID / ADS-B In:** only the message plumbing can be mocked. Bench-test real compliance (see section 6).
- **OTA updates:** simulate in a container or on a spare Jetson. Bench-test power-loss and rollback.
- **Night light, weather sensors:** mock topics. Bench-test the real hardware.
- **Failsafes (RTL, geofence, low battery, animal escape):** simulate everything first, then repeat staged and tethered on the real airframe.

## 3. Failsafe scenarios and pass/fail criteria
Each scenario below is also encoded in the demo (section 4). Run the same names against PX4 SITL and compare the behavior.
1. **Animal appears → climb/escape → RTL.** Pass if three consecutive confident detections (confidence ≥ 0.6) trigger a +15 m climb, then RTL, then landing. Fail if a single false-positive frame triggers an escape.
2. **Geofence breach.** Pass if RTL starts within one tick of leaving the fence and overshoot stays within 1 m (demo: 40 mm).
3. **Low battery.** Pass if RTL starts at 25 % and the landing sequence completes. A critical level (15 %) forces landing in place if still airborne.
4. **GPS loss.** Pass if loss lasting 2 s forces a straight-down landing, because RTL needs a position. Fail if it tries RTL.
5. **Link loss.** Pass if 3 s without a link starts RTL and the link coming back does not cancel it.
6. **Sensor dropout (stale frame gate).** Pass if a detector frame older than 200 ms blocks the escape trigger (hold) and escape resumes within 15 ticks of frames returning; if the detector is silent for 4 s, RTL starts. Same idea as the stale-frame gate in the camera lessons.
7. **Landing → cover → panels.** Pass if the cover command is only issued after landing detection (altitude ≤ 0.3 m), the panels deploy only after the cover is open, and the gear is down below 5 m on descent. Any command in the air is a violation.
8. **Wind gusts.** Pass if seeded gusts of ±5 m/s never push the vehicle outside the fence margin and the mission still completes. In PX4 SITL, use Pegasus or Gazebo wind settings and check position error and battery drain.

For real PX4 parameters use [Safety Configuration (Failsafes)](https://docs.px4.io/main/en/config/safety.html) (it lists, for example, `COM_LOW_BAT_ACT`, `COM_GNSSLOSS_ACT` and `COM_DL_LOSS_T`), [Return Mode](https://docs.px4.io/main/en/flight_modes_mc/return.html), and the [parameter reference](https://docs.px4.io/main/en/advanced_config/parameter_reference.html). Our companion node (the Jetson supervisor) must command *through* PX4's failsafes, never fight them.

## 4. Runnable example: deterministic failsafe state machine
Files in `2026-10-18-drone-failsafe-sim/code/`:
```bash
g++ -std=c++17 -O2 -Wall -Wextra failsafe_sim.cpp -o failsafe_sim
python3 failsafe_sim.py > py.txt && ./failsafe_sim > cpp.txt && diff cpp.txt py.txt && echo IDENTICAL
```
Result (real run): 91 lines, byte-identical, `TOTAL pass=10/10`. It uses integer math only (mm, 20 ms ticks, 0.01 % battery) and the same seeded LCG in both languages (seed 2026 plus scenario number), so there is no float rounding to disagree about. It is a model of the *supervisor logic*, not of flight physics, and it does not replace PX4.

Highlights from the run:
- **Animal:** ANIMAL fires at tick 1010 (three confirming frames), climb to 45 m, ESCAPE_DONE at 1384, then RTL, landing at 3683, cover, panels, done at 4083.
- **False positive:** one 0.65 frame at tick 750 is ignored. The run ends with MISSION_END at 3000.
- **Geofence:** the fence is crossed at tick 1667 with 120.04 m max distance.
- **Low battery:** BATT_LOW at 1750. **GPS loss:** straight landing, trigger at 2099, with no RTL state visited. **Link loss:** RTL at 2149 and still lands after the link returns.
- **Short dropout (2.5 s):** 119 held ticks, then the escape fires at 1635, ten ticks after frames return. **Long dropout (6 s):** DET_STALE RTL at 1695.
- **Gust:** max distance 101.64 m, well inside the fence. Zero cover/panel violations in all ten.

## 5. CI / DevOps hand-off
- **Config:** `drone_failsafe_gate.yaml` mirrors the constants (the demo hard-codes them, so CI should parse the YAML and fail on drift). Keep real PX4 parameters in a separate versioned params file exported from QGroundControl.
- **Golden results:** `golden.txt` has the first trigger and done tick per scenario, and `run_ci.sh` runs both languages, diffs them, and requires 10/10. Run it on every commit. Add a boundary test for each threshold (the camera lessons found a 0.1 s float edge; integers avoid it here).
- **SITL in Docker:** PX4 publishes the [`px4-dev` container](https://docs.px4.io/main/en/test_and_ci/docker.html) (`ghcr.io/px4/px4-dev`, `px4io/px4-dev`) for building and running SITL. That page says a `px4-sim` image with Gazebo is *planned*, so for now build your own image from it. I did not run PX4 SITL in Docker for this guide. Isaac Sim has its [container install](https://docs.isaacsim.omniverse.nvidia.com/latest/installation/install_container.html), which needs an NVIDIA GPU runner. Run the fast logic tests on every commit and the Isaac Sim scenes nightly.
- **Git:** version scenes (USD), params, YAML and golden files. Keep Starlink keys, signing keys and Remote ID serial credentials out of git.

## 6. Safety and legal
Passing in simulation proves the logic can work; it does not prove the aircraft is safe. Sim has no vibration, magnetic interference, real GNSS multipath, motor/ESC faults, battery sag, wildlife behavior or weather.
- Fly the real airframe in stages: props off bench test, tethered hover, low-altitude short hops, then the real failsafes one at a time, in an approved open area, with a manual override and a spotter.
- **FAA rules.** For commercial flights in the US, read [14 CFR Part 107](https://www.ecfr.gov/current/title-14/chapter-I/subchapter-F/part-107), the FAA's [commercial operators page](https://www.faa.gov/uas/commercial_operators) and [Part 107 waivers](https://www.faa.gov/uas/commercial_operators/part_107_waivers). Beyond-visual-line-of-sight, night, and an autonomous drone that deploys parts and flies animal-escape maneuvers may need a waiver or other approvals.
- **Remote ID** is required for most US drones: see the FAA's [Remote ID page](https://www.faa.gov/uas/getting_started/remote_id) and [14 CFR Part 89](https://www.ecfr.gov/current/title-14/chapter-I/subchapter-F/part-89). PX4's [safety configuration page](https://docs.px4.io/main/en/config/safety.html) lists a Remote ID arming check (`COM_ARM_ODID`); confirm your module's compliance with its manufacturer.
- Wildlife rules (disturbing animals) and local land rules also apply. Check them before the first field test. This is not legal advice.

## 🧰 AI Tools in This Lesson
- **PyTorch / ONNX / TensorRT:** actually used on one step only. You take frames from Replicator, run them through the real detector on the Jetson (TensorRT engine built from ONNX exported from PyTorch), and check the escape logic reacts. See [PyTorch ONNX export](https://pytorch.org/docs/stable/onnx.html), [ONNX Python API](https://onnx.ai/onnx/api/index.html) and [TensorRT docs](https://docs.nvidia.com/deeplearning/tensorrt/latest/). The code files use none of the three.
- **ROS 2:** the Isaac Sim ROS 2 bridge publishes RTX Lidar and camera topics, and the mocks (cover, gear, battery, Starlink) are topics. [ROS 2 docs](https://docs.ros.org/en/jazzy/index.html).
- Algebra to know: confirm-count logic `cnt ← cnt+1 if conf ≥ 0.6 else 0`; distance from home `d = √(x²+y²)`; time to home `t = d / v` (100 m at 5 m/s = 20 s); a tick count is `t / 0.02`.

## 🛠️ Other Skills in This Lesson
- **Bash:** the build/run/diff line in section 4 and `run_ci.sh`. [W3Schools Bash](https://www.w3schools.com/bash/)
- **YAML:** `drone_failsafe_gate.yaml`. [YAML spec](https://yaml.org/spec/) (no W3Schools YAML page returned 200)
- **Git:** versioning scenes, params and golden files. [W3Schools Git](https://www.w3schools.com/git/)
- **Docker:** PX4 and Isaac Sim containers in section 5. [Docker get started](https://docs.docker.com/get-started/) (no W3Schools Docker page returned 200)
- **OpenUSD:** the Isaac Sim scene, drone asset and sensor prims are USD. [Learn OpenUSD](https://docs.nvidia.com/learn-openusd/latest/index.html)
- **URDF:** not used here; Pegasus vehicles are USD assets.
- **XML:** not used in the demo. PX4 and Gazebo models use SDF/XML files, which this guide doesn't edit. [W3Schools XML](https://www.w3schools.com/xml/)
- **Math beyond algebra:** statistics (seeded pseudo-random gusts and confidences; [Khan Academy statistics](https://www.khanacademy.org/math/statistics-probability)); trigonometry for geofence and heading ([Khan Academy trigonometry](https://www.khanacademy.org/math/trigonometry)). Python/C++ practice: [Python](https://www.w3schools.com/python/), [C++](https://www.w3schools.com/cpp/).

## 🎓 NVIDIA Cert Note
Scene, drone asset and sensor prims are OpenUSD, which maps to the [NCP-OUSD OpenUSD Development Professional](https://www.nvidia.com/en-us/learn/certification/openusd-development-professional/) exam. Free prep: [Learn OpenUSD](https://docs.nvidia.com/learn-openusd/latest/index.html).
