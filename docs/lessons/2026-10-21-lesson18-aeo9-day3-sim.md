# Lesson 18 — MISSION AEO-9, Day 3 in Isaac Sim: the optional film day at camp 2
*Isaac Sim course, now numbered course 12 of 14 ("12-Develop & Ship"). One-off, Wed 2026-10-21 ("Run once 3rd day plan"). The weekday routine `isaac-sim-weekday-lesson` stays paused.*
Builds on Lesson 17 (`2026-10-20-lesson17-aeo9-mission-sim.md`, 5-day supervisor) and today's Day 3 lessons from Jetson L20, Framework L22, Library L22, R-Time L11, C-Motion and SI L9. Code: `2026-10-21-lesson18-code/`.

## ⚠️ The park rule, plainly
Drones are prohibited in Grand Canyon National Park without authorization. Under [36 CFR 1.5](https://www.ecfr.gov/current/title-36/chapter-I/part-1/section-1.5) and NPS Policy Memo 14-05, launching, landing or operating uncrewed aircraft on park land is banned unless the park gives written approval ([NPS: uncrewed aircraft in the national parks](https://www.nps.gov/articles/unmanned-aircraft-in-the-national-parks.htm), [Grand Canyon laws & policies](https://www.nps.gov/grca/learn/management/lawsandpolicies.htm), [compendium](https://www.nps.gov/grca/learn/management/compendium.htm)). Camp 2 must be on land you are allowed to use, outside the park. [Part 107](https://www.ecfr.gov/current/title-14/chapter-I/subchapter-F/part-107) and [Remote ID](https://www.faa.gov/uas/getting_started/remote_id) still apply. Not legal advice. In the sim, "Grand Canyon" is only terrain. October is software-only; hardware starts in November.

## Goals
- Replay Day 3 minute by minute: dawn state → go/no-go → stow → climb → film loop → escape → RTL → deploy → upload → recharge → sleep → Day 4 readiness.
- Make the go/no-go explicit: wind, energy plus reserve, and **skip Day 3 if the trip is behind**.
- Test five Day-3 failsafes plus the skip path, and show how each changes Day 4.
- Know what Isaac Sim covers (Replicator wildlife, RTX Lidar clearance) and what it cannot (thermal).

## Day 3 timeline (sim; every number an illustrative placeholder)
- **00:00–06:30 night:** Jetson asleep (1 dWh/min, dWh = 0.1 Wh). Dawn SoC comes from the BMS mock (`/aeo9/bms`), 3200 dWh of 4000.
- **06:30 on:** solar on a triangle daylight curve (peak 10 dWh/min at 12:15, panels deployed since Day 2).
- **08:00 go/no-go (window to 14:00):** behind schedule → SKIP; wind > 11.0 m/s → wait; SoC ≥ flight need 2330 + reserve 800 → GO. Nominal GO at 10:29 once the morning sun tops up.
- **Stow (3 min, C-Motion) → climb (5 min) → film loop (15 min, 120 m/min around camp 2, 75 m above camp)**. Each film minute sends a mocked `/sony/trigger` and expects an ack. Replicator frames feed the detector; a confident close detection fires a +35 m escape climb.
- **RTL camp 2 (5 min) → land → deploy cover and panels (3 min) → Starlink upload (60 link-minutes, deadline 20:00) → afternoon charge → 18:00 sleep.**
- **Day 4 readiness:** from 08:00 on Day 4, the first minute SoC ≥ Day-4 need 2280 + reserve 800 is the earliest Day 4 GO.

## Docs
[Pegasus Simulator](https://pegasussimulator.github.io/PegasusSimulator/) · [PX4 ROS 2 guide](https://docs.px4.io/main/en/ros2/user_guide.html) · [MAVSDK](https://mavsdk.mavlink.io/main/en/index.html) · [PX4 safety/failsafes](https://docs.px4.io/main/en/config/safety.html) · [PX4 Return mode](https://docs.px4.io/main/en/flight_modes_mc/return.html) · [PX4 terrain following](https://docs.px4.io/main/en/flying/terrain_following_holding.html) · [RTX Lidar](https://docs.isaacsim.omniverse.nvidia.com/latest/sensors/isaacsim_sensors_rtx_lidar.html) · [Replicator tutorials](https://docs.isaacsim.omniverse.nvidia.com/latest/replicator_tutorials/index.html) · [Replicator extension](https://docs.omniverse.nvidia.com/extensions/latest/ext_replicator.html) · [Isaac Sim ROS 2 tutorials](https://docs.isaacsim.omniverse.nvidia.com/latest/ros2_tutorials/index.html) · terrain tiles: [The National Map](https://apps.nationalmap.gov/downloader/).

**Demo video (checked):** [Pegasus Simulator — small introduction demo](https://www.youtube.com/watch?v=_11OCFwf_GE).

## Isaac Sim pieces for Day 3
- **Replicator wildlife, day vs dusk:** randomize animal assets, poses and distance; randomize the DistantLight angle/intensity for midday vs dusk (low sun, long shadows). Write frames, replay them through the real TensorRT detector on the Jetson, and report recall separately for day and dusk. Expect dusk to be worse; that number decides whether Day 3 filming should end before golden hour.
- **RTX Lidar terrain clearance:** a Mid-360-like RTX Lidar config (custom; no verified Livox pattern) reports min range below the drone; compare to the code's AGL check (min 40 m, placeholder). The code's ridge gives 45 m nominal.
- **Thermal gap:** Isaac Sim has no native thermal sensor (per L17's check). A pseudo-thermal render (animals bright, rocks dark) only tests plumbing, never real thermal detection. Night watch stays off by default (Library L22, SI L9).

## Code (run; Python and C++ outputs identical)
`aeo9_day3.py`, `aeo9_day3.cpp`, `aeo9_day3.yaml`, `golden.txt`, `run_ci.sh`. 1-minute ticks from Day 3 00:00 to Day 4 14:00, integer math, LCG seed 2026 + scenario index. Each run also writes the **minute-by-minute energy budget** CSV (15,967 rows: scenario, minute, state, draw, solar, SoC); CI byte-compares the two CSVs too.
```bash
cd 2026-10-21-lesson18-code && ./run_ci.sh   # diff stdout, cmp CSVs, diff golden, needs 7/7 -> "CI OK"
```
Key lines:
```python
need = 5 * CRUISE + FILM_MIN * FILM + 2 * ESC + 5 * CRUISE      # climb + film + 2 escapes + return
if behind: st = "CHARGE"                                          # skip optional Day 3
if g > GUST_ABORT: reason = "gust %d dm/s" % g
elif esc >= MAX_ESC: reason = "escape limit"
elif fails >= CAM_FAILS: reason = "camera trigger fail x%d" % fails
```
What you'll see (58 lines, 7/7 PASS):
- **nominal:** GO 10:29, 15 min filmed, landed 10:57, upload done 12:00, min AGL 45 m, Day 4 earliest GO 13:14.
- **gust_filming:** a 15.0 m/s gust at film minute 12 → RTL; Day 4 GO 12:42.
- **repeat_escapes:** 5 detections, 3 escapes → escape limit RTL at 10:48; Day 4 GO 13:01.
- **starlink_outage:** 150 min link down; upload finishes 14:29 instead of 12:00, before the 20:00 deadline.
- **cloudy_afternoon:** 25 % sun after 14:00; Day 4 never reaches need by 14:00 → **D4_HOLD_CHARGE** (expected; the supervisor must use a rest day, and L17's re-plan logic decides if home on Day 5 still holds).
- **camera_trigger_fail:** acks stop at minute 7; three misses → RTL at 10:46 with 9 min filmed.
- **behind_schedule:** SKIP at 08:01, Day 4 GO 08:00 sharp.
Lesson: Day 3 filming costs Day 4 its morning. That's the trade the go/no-go is weighing.

## CUDA / GPU angle
Put thousands of Day-3 weather seeds (wind, cloud, detections) in one GPU tensor, roll the energy forward with a cumulative sum, and copy back only the Day 4 GO minute per seed. Replicator renders on the GPU; on the Jetson, YOLO runs as a TensorRT engine. Keep host↔device copies few and use streams. [CUDA Best Practices](https://docs.nvidia.com/cuda/cuda-c-best-practices-guide/index.html) · [CUDA streams](https://docs.nvidia.com/cuda/cuda-runtime-api/group__CUDART__STREAM.html) · [TensorRT](https://docs.nvidia.com/deeplearning/tensorrt/latest/).

## W3Schools practice
C++: [switch](https://www.w3schools.com/cpp/cpp_switch.asp), [files](https://www.w3schools.com/cpp/cpp_files.asp), [random numbers](https://www.w3schools.com/cpp/cpp_howto_random_number.asp). Python: [random module](https://www.w3schools.com/python/module_random.asp), [writing files](https://www.w3schools.com/python/python_file_write.asp), [string formatting](https://www.w3schools.com/python/python_string_formatting.asp).

## Hardware note (November; verified prices only)
Livox's [Mid-360 page](https://www.livoxtech.com/mid-360) lists $749 (re-checked today). No other part price is verified here; get written quotes for Pixhawk, thermal camera, Sony ZV-E10 II, Starlink Mini, panels, MPPT and BMS.

**Industries:** wildlife surveys, documentary filming, search and rescue, inspection, long-range mapping.

## Sim → deploy (ROS 2 / MAVLink)
In sim, PX4 SITL in Pegasus talks MAVLink; the supervisor only requests hold, RTL or land (via MAVSDK or PX4's ROS 2 interface), and PX4's own failsafes stay in charge. On the drone, the same supervisor reads real `/aeo9/bms`, MPPT, weather and Starlink topics instead of mocks; `/sony/trigger` becomes the real camera driver bench-tested in November. Deploy validated configs; nothing here goes to the airframe untested.

## Labs (~45 min)
1. Run `./run_ci.sh`.
2. Plot `budget_py.csv` for nominal vs cloudy_afternoon.
3. Raise `MAX_ESC` to 5 in both files. What does Day 4 lose?
4. Make the gust 1.0 m/s weaker. Does filming finish?
5. Add a rule: no GO on Day 3 if the projected Day 4 GO is after 12:00.
6. In Isaac Sim, render 200 day and 200 dusk Replicator frames and compare detector recall.
7. Stretch: fly the film loop in Pegasus with an RTX Lidar and log min range.

## Exit ticket
1. List Day 3's go/no-go checks in order.
2. Why does the cloudy afternoon hurt Day 4 more than the gust?
3. What does a pseudo-thermal render prove and not prove?
4. Why must the camera trigger failure end filming, not the flight in place?
5. State the park rule and its regulation.

## Hand-off to DevOps (course 9)
Version `aeo9_day3.yaml` + `golden.txt`; CI runs `run_ci.sh` (stdout diff, CSV cmp, 7/7). The demo hard-codes thresholds; real CI should load the YAML and fail on drift. Nightly GPU job: one Pegasus Day 3 flight plus the Replicator day/dusk recall report. Use PX4's [Docker docs](https://docs.px4.io/main/en/test_and_ci/docker.html) for SITL.

## Hand-off to the other courses
- **Jetson (1):** reconcile its 19.6 GB/day budget with the 60-min upload window; the outage case finishes 2.5 h late.
- **Framework (2/4):** add `D4_HOLD_CHARGE` as a phase outcome next to its HOLD code; keep the Day 3 skip flag through reboot.
- **Library (3):** Monte Carlo of Day 3 seeds on GPU; Replicator loader with day/dusk tags.
- **R-Time (4):** real timestamped BMS/MPPT minutes to replace the curve.
- **C-Motion (5):** stow before arm and deploy only after landing, as modeled (3 min each).
- **SI (6/7):** measured draws, solar and real failsafe thresholds; decide who sends RTL to PX4.

## 🧰 AI Tools in This Lesson
- **[PyTorch](https://pytorch.org/tutorials/):** in the GPU angle, `soc = soc0 - torch.cumsum(draw - solar, dim=1)` rolls every seed's budget at once. API: [torch.cumsum](https://pytorch.org/docs/stable/generated/torch.cumsum.html).
- **[ONNX](https://onnx.ai/):** the YOLO animal model is exported as `yolo_animals.onnx`. API: [ONNX Runtime](https://onnxruntime.ai/docs/api/).
- **[TensorRT](https://docs.nvidia.com/deeplearning/tensorrt/latest/):** `trtexec --onnx=yolo_animals.onnx --saveEngine=yolo_animals.plan --fp16` builds the Jetson engine that scores Replicator frames.
- **[ROS 2](https://docs.ros.org/en/jazzy/Concepts/Basic/About-Topics.html):** `/aeo9/bms` (`BatteryState`), lidar `PointCloud2`, camera `Image`, mocked `/sony/trigger`. API: [sensor_msgs](https://docs.ros.org/en/jazzy/p/sensor_msgs/).
- The two code files use none of these.

**Algebra to know**
- Energy: \( E_{t+1}=\min(C,\max(0,E_t-d_t+s_t)) \).
- Launch gate: \( E \ge E_{need}+E_{res} \), with \( E_{need}=5c+15f+2e+5c \).
- Solar: \( s(t)=P\cdot\frac{345-|t-735|}{345}\cdot\frac{k}{100} \) for sunrise < t < sunset.
- Clearance: \( h_{AGL}=h_{alt}-z_{terrain}(x)\ge 40 \).
- Recall: \( R=\frac{TP}{TP+FN} \), compared for day vs dusk.

## 🛠️ Other Skills in This Lesson
- **Bash:** `run_ci.sh` with `set -euo pipefail`, `diff`, `cmp`. [W3Schools Bash](https://www.w3schools.com/bash/)
- **YAML:** `aeo9_day3.yaml` holds the gate. [YAML spec](https://yaml.org/spec/1.2.2/)
- **OpenUSD:** the terrain, DistantLight and sensor prims of the Day 3 stage. [Learn OpenUSD](https://docs.nvidia.com/learn-openusd/latest/index.html)
- **Git:** commit YAML + golden together; keep Starlink keys and footage out of git. [W3Schools Git](https://www.w3schools.com/git/)
- **Docker:** PX4 SITL container in nightly CI. [PX4 Docker](https://docs.px4.io/main/en/test_and_ci/docker.html)
- **Math beyond algebra:** discrete integration of power over time ([Khan Academy calculus](https://www.khanacademy.org/math/calculus-1)); seeded randomness and recall ([Khan Academy statistics](https://www.khanacademy.org/math/statistics-probability)).
- Not used: XML, URDF.

## 🎓 NVIDIA Cert Note
Building the Day 3 stage (terrain, sun angle for day vs dusk, lidar and camera prims) is practice for [NCP-OUSD OpenUSD Development Professional](https://www.nvidia.com/en-us/learn/certification/openusd-development-professional/) ($200). Free prep: [Learn OpenUSD](https://docs.nvidia.com/learn-openusd/latest/index.html). Good second step: DLI [C-OV-05](https://learn.nvidia.com/courses/course-detail?course_id=course-v1:DLI+C-OV-05+V1).
