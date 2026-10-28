# Lesson 17 — MISSION AEO-9 in Isaac Sim: rehearsing a 5-day Grand Canyon wildlife-filming trip
*Isaac Sim course, one-off (Isaac's "course 7 of 14, run in order"), Tue 2026-10-20. The weekday routine stays paused.*
Builds on `2026-10-18-drone-failsafe-sim.md` (Pegasus + PX4 SITL, 10 failsafe scenarios). Code: `2026-10-20-lesson17-code/`.

## ⚠️ Read first: the park rule
Grand Canyon is NPS land. NPS Policy Memorandum 14-05 directs superintendents to use [36 CFR 1.5](https://www.ecfr.gov/current/title-36/chapter-I/part-1/section-1.5) to **prohibit launching, landing or operating uncrewed aircraft** on park lands and waters, and the NPS says that is still in force ([NPS: Uncrewed aircraft in the national parks](https://www.nps.gov/articles/unmanned-aircraft-in-the-national-parks.htm); park rules: [Grand Canyon laws & policies](https://www.nps.gov/grca/learn/management/lawsandpolicies.htm), [compendium](https://www.nps.gov/grca/learn/management/compendium.htm)). The NPS page even names drones "lost over the edge of the Grand Canyon". AEO-9 cannot launch, land, camp or film inside the park without written NPS authorization. Plan the real trip on land outside the park boundary (and check that land manager's rules), and FAA rules still apply ([Part 107](https://www.ecfr.gov/current/title-14/chapter-I/subchapter-F/part-107), [Remote ID](https://www.faa.gov/uas/getting_started/remote_id)). Not legal advice. In simulation, "Grand Canyon" is just terrain.

## Goals
- Turn the 10 one-flight failsafe scenarios into a **5-day mission** supervisor: PREFLIGHT → STOW → TRANSIT → FILM (with animal escape) → LAND → DEPLOY/CHARGE → overnight sleep, repeated per day.
- Keep an **energy budget**: launch only if `need + reserve` is in the pack; abort filming at a floor that still reaches the camp.
- Model **solar recharge and day/night** (illustrative numbers), and **re-plan**: drop optional filming days when weather or clouds put you behind, so AEO-9 still gets home on day 5.
- Check **geofence corridor** and **terrain clearance (AGL)**.
- Know which part runs in Isaac Sim/Pegasus, which is mocked, and what November hardware must prove.

## The 5-day plan and what the sim does in each phase
Route (sim only): home (camp 0) → camp 1 at 6 km → camp 2 at 12 km. Legs: day 1 home→camp 1 + 12 min film; day 2 camp 1→camp 2 + 12 min; day 3 film-only at camp 2 (20 min, **optional**); day 4 camp 2→camp 1 + 8 min; day 5 camp 1→home + 5 min.
- **Launch (each morning, flight window 08:00–14:00 launch, airborne only 08:00–17:00):** PX4 SITL preflight via MAVSDK; mocked `/aeo9/weather` wind vs. limit; energy check from mocked `/aeo9/bms`; C-Motion's stow sequence (3 min) must finish before arming.
- **Transit:** Pegasus multirotor flies the waypoint leg at 110 m above home; the RTX Lidar (Mid-360 approximation) watches the terrain; a crosswind drift pushes the corridor and the supervisor steers back at the soft fence.
- **Filming:** gimbal-camera trigger is a mocked topic; Replicator renders animal frames (day RGB; pseudo-thermal at dusk) that you replay into the real TensorRT detector on a Jetson; a confident close detection triggers a 2-minute escape climb (the 2026-10-18 failsafe logic).
- **Land + overnight solar recharge:** land, C-Motion deploys cover and panels (landed only), Starlink upload mock for 60 min, then solar charge on a daylight curve; at night the Jetson sleeps (idle draw ¼). Honest note: panels charge in daylight; "overnight" means parked and asleep, then the morning sun tops up before launch.
- **Return (day 5):** last leg home; mission passes only if every mandatory leg is done.
- **Days 1–5 in Isaac Sim:** run each day as a separate scene with a USD stage time of day (DistantLight angle) and saved state (SoC, camp) passed to the next run, or run the integer supervisor across all 5 days at 1-min ticks (today's code) and use Isaac Sim for the flown segments.

## Docs
- [Pegasus Simulator](https://pegasussimulator.github.io/PegasusSimulator/) (v5.1.0 ↔ Isaac Sim 5.1.0, PX4 1.14.3) · [PX4 ROS 2 user guide](https://docs.px4.io/main/en/ros2/user_guide.html) · [MAVSDK](https://mavsdk.mavlink.io/main/en/index.html)
- [PX4 terrain following/holding](https://docs.px4.io/main/en/flying/terrain_following_holding.html) · [PX4 parameter reference](https://docs.px4.io/main/en/advanced_config/parameter_reference.html)
- [Isaac Sim RTX Lidar](https://docs.isaacsim.omniverse.nvidia.com/latest/sensors/isaacsim_sensors_rtx_lidar.html) · [Isaac Sim ROS 2 tutorials](https://docs.isaacsim.omniverse.nvidia.com/latest/ros2_tutorials/index.html) · [Replicator tutorials](https://docs.isaacsim.omniverse.nvidia.com/latest/replicator_tutorials/index.html) · [Replicator extension](https://docs.omniverse.nvidia.com/extensions/latest/ext_replicator.html)
- **Terrain (TODO):** real DEM import is not done here. USGS elevation data can be downloaded from [The National Map downloader](https://apps.nationalmap.gov/downloader/); converting a DEM GeoTIFF to a USD heightfield mesh is a lab, not a verified pipeline. The code uses an invented ridge profile.

## Demo videos (checked working)
- [Pegasus Simulator - Small introduction demo](https://www.youtube.com/watch?v=_11OCFwf_GE) (Marcelo Jacinto, Pegasus author)
- [Pegasus Simulator: An Isaac Sim Framework for Multiple Aerial Vehicles Simulation](https://www.youtube.com/watch?v=caFPdl1rOT4)

## Code (run; Python and C++ outputs identical)
`aeo9_mission.py`, `aeo9_mission.cpp`, `aeo9_mission.yaml`, `golden.txt`, `run_ci.sh`. 1-minute ticks, 1440 per day × 5 days × 5 scenarios, integer math, LCG seed 2026+scenario id. **Every number (400 Wh pack, 10 Wh/min cruise, 60 W solar peak, 11 m/s wind limit, corridor, terrain) is an illustrative placeholder, not an AEO-9 or vendor spec.**
```bash
cd 2026-10-20-lesson17-code && ./run_ci.sh   # builds C++, diffs vs Python and golden, needs 5/5 -> "CI OK"
```
Key lines:
```python
need = (abs(CAMPS[b] - CAMPS[a]) // SPEED + 1) * CRUISE_W + fm * FILM_W + 3 * ESC_W
if wind > WIND_MAX: reason = "wind"          # scrub, retry every minute until 14:00
elif soc < need + RESERVE: reason = "charging"
while leg < len(LEGS) and LEGS[leg][3] and mand_left >= 6 - day:   # behind -> drop optional day
    leg += 1
if film_left <= 0 or soc < FLOOR + RESERVE // 2 + dist_home // SPEED * CRUISE_W: state = "LAND"
```
```cpp
long solar(long t,long cloud){ if(t<=SUNRISE||t>=SUNSET) return 0;
  return SOLAR_PEAK*(345-labs(t-NOON))*cloud/(345*100); }   // triangle daylight curve
```
What you'll see (39 lines, 5/5 PASS):
- **nominal:** five flights, all landed by 10:24; lowest SoC 25 %; AGL 50–110 m (the 60 m ridge).
- **cloudy_day1** (35 % sun): day 1 ends at 42 %, day 2 is SCRUB (charging), day 3 flies leg 1, day 4 drops the optional film day, home on day 5.
- **wind_day2** (14 m/s): day 2 SCRUB (wind), optional day dropped on day 4, home on day 5.
- **animal_heavy:** 9 detections and 2 escapes on day 3; escapes cost energy, so day 4 launches at 10:27 after charging; min SoC 23 %.
- **fence_drift:** crosswind on leg 3 pushes up to 1190 m off the line; the soft-fence correction keeps it inside 1500 m.

## CUDA / GPU angle
Isaac Sim renders RTX Lidar and camera on the GPU; Replicator generates the animal dataset on the GPU. For Monte Carlo of the 5-day budget, put thousands of weather/cloud seeds in one tensor and roll the energy forward on the GPU, then only copy back the pass/fail summary (few host↔device copies, use streams). On the Jetson, the YOLO detector runs as a TensorRT engine.
[CUDA C++ Best Practices](https://docs.nvidia.com/cuda/cuda-c-best-practices-guide/index.html) · [CUDA streams](https://docs.nvidia.com/cuda/cuda-runtime-api/group__CUDART__STREAM.html) · [TensorRT](https://docs.nvidia.com/deeplearning/tensorrt/latest/) · [Isaac Sim container](https://docs.isaacsim.omniverse.nvidia.com/latest/installation/install_container.html)

## W3Schools practice
C++: [while loops](https://www.w3schools.com/cpp/cpp_while_loop.asp), [switch](https://www.w3schools.com/cpp/cpp_switch.asp), [strings](https://www.w3schools.com/cpp/cpp_strings.asp), [random numbers](https://www.w3schools.com/cpp/cpp_howto_random_number.asp). Python: [while loops](https://www.w3schools.com/python/python_while_loops.asp), [functions](https://www.w3schools.com/python/python_functions.asp), [random module](https://www.w3schools.com/python/module_random.asp).

## Real hardware note (November)
- **Livox Mid-360:** Livox's own [Mid-360 page](https://www.livoxtech.com/mid-360) says "the price of the MID-360 is only $749". Confirm with a reseller (shipping, cable, tax).
- **Jetson Orin Nano Super dev kit:** [NVIDIA product page](https://www.nvidia.com/en-us/autonomous-machines/embedded-systems/jetson-orin/nano-super-developer-kit/) verified; I could not read a price on it today, so treat price as unverified — check a listed reseller.
- Pixhawk, thermal camera, Sony ZV-E10 II, Starlink Mini, M10 GPS, 6S Li-ion, panels, MPPT, BMS: prices not verified here.
- **Industries:** wildlife and conservation survey, film/documentary, search and rescue, infrastructure and pipeline inspection, precision agriculture, long-endurance mapping.

## Sim → deploy (ROS 2 / MAVLink)
Same supervisor code, swap inputs: mocked `/aeo9/bms`, `/aeo9/solar`, `/aeo9/weather`, `/aeo9/uplink` (proposed names) become the real BMS/MPPT/weather drivers; PX4 SITL becomes the Pixhawk over MAVLink (MAVSDK or PX4's ROS 2 bridge); Replicator frames become the live camera and thermal topics. The supervisor only **requests** modes (RTL, land, hold); PX4 failsafes stay authoritative.

## Labs (~45 min)
1. Run `./run_ci.sh`; confirm "CI OK".
2. Set cloud to 35 on days 1 *and* 2. Does AEO-9 still get home? Which day becomes the failure point?
3. Raise `RESERVE` to 1000. Which scenario starts scrubbing for "charging"?
4. Change `p_animal` to 2 in animal_heavy. Watch the in-flight floor force early landings.
5. In Pegasus, fly the day-1 leg with PX4 SITL and log battery; compare Wh used to the 208–236 Wh model.
6. Stretch: download a DEM tile from The National Map, make a heightfield in USD, and replace `terrain()`.

## Exit ticket
1. Write the launch rule in words. Why is the reserve separate from the in-flight floor?
2. Why does solar get counted only while GROUND?
3. In wind_day2, which leg is dropped and why is that the right one?
4. What rule stops AEO-9 from launching or landing in Grand Canyon National Park, and which link proves it?
5. Name two things Isaac Sim can't honestly simulate for AEO-9.

## Hand-off to course 9 (DevOps)
Version `aeo9_mission.yaml` (the code still hard-codes the values; CI should load YAML and fail on drift). Keep `golden.txt` (39 lines) and `run_ci.sh` (C++/Python diff + golden diff + 5/5). Nightly: Pegasus day-1 leg on a GPU runner. Record each simulated day as a bag plus SoC/camp state file so day N+1 replays from day N. Keep Starlink credentials, park permits, GPS home coordinates of real camps and tokens out of git.

## Hand-offs to the other courses
- **1 Jetson & JetPack:** add AEO-9 day-cycle states (SLEEP/WAKE) to Contract v2.1; Jetson owns the night sleep draw and the morning wake time.
- **2 Framework:** heartbeat/watchdog around the mission supervisor; persist day state across reboots.
- **3 Library:** batched GPU Monte Carlo of the energy model; Replicator dataset loader for the detector.
- **4 R-Time:** real `/aeo9/bms` and MPPT readers with timestamps; stale BMS = no launch.
- **5 C-Motion:** stow before arm (3 min in the sim), deploy only when landed; report done/fault.
- **6 SI:** replace every placeholder with measured values (pack Wh, cruise W, panel W, wind limit) and the PX4 failsafe parameters.

## 🧰 AI Tools in This Lesson
The two code files use none of these; they appear in the GPU/deploy steps.
- [PyTorch](https://pytorch.org/docs/stable/tensors.html) — GPU Monte Carlo (sketch, not run): `soc = cap - torch.cumsum(draw - solar, dim=1)` over a `[seeds, minutes]` tensor (no clamp at full, so it's an upper bound on stored energy). API: [torch.cumsum](https://pytorch.org/docs/stable/generated/torch.cumsum.html), [torch.Tensor](https://pytorch.org/docs/stable/tensors.html).
- [ONNX](https://onnx.ai/) — export the YOLO animal model to `yolo_animals.onnx`; check it with [ONNX Runtime](https://onnxruntime.ai/docs/api/).
- [TensorRT](https://docs.nvidia.com/deeplearning/tensorrt/latest/) — `trtexec --onnx=yolo_animals.onnx --saveEngine=yolo_animals.plan --fp16` on the Jetson (read, not run).
- [ROS 2](https://docs.ros.org/en/jazzy/p/sensor_msgs/) — Isaac Sim publishes `sensor_msgs/PointCloud2` (lidar) and `Image` (Replicator frames); the mocked BMS maps to `sensor_msgs/BatteryState`.

Algebra to know:
- **Energy budget:** \( E_{need} = \left(\lfloor d/v \rfloor + 1\right)P_{c} + t_f P_f + 3P_e \), launch if \( E \ge E_{need} + E_{res} \).
- **Solar curve:** \( P(t) = P_{pk}\,\frac{345-|t-735|}{345}\,\frac{c}{100} \) for sunrise < t < sunset.
- **State of charge:** \( E_{t+1} = \min(E_{cap},\, E_t + P_{sun} - P_{load}) \).
- **AGL:** \( h_{AGL} = h_{flight} - h_{terrain}(x) \ge 40 \).

## 🛠️ Other Skills in This Lesson
- **Bash:** `run_ci.sh` with `set -euo pipefail`, `diff`, `grep -q`. [W3Schools Bash](https://www.w3schools.com/bash/)
- **YAML:** `aeo9_mission.yaml` thresholds. [YAML 1.2.2 spec](https://yaml.org/spec/1.2.2/), [ROS 2 parameters](https://docs.ros.org/en/jazzy/Tutorials/Beginner-CLI-Tools/Understanding-ROS2-Parameters/Understanding-ROS2-Parameters.html)
- **OpenUSD:** terrain heightfield, sun DistantLight per day, camp prims. [Learn OpenUSD](https://docs.nvidia.com/learn-openusd/latest/index.html)
- **Git:** golden file in git; secrets and real coordinates out. [W3Schools Git](https://www.w3schools.com/git/)
- **Docker:** Isaac Sim container for the nightly runner. [Docker docs](https://docs.docker.com/)
- **Math beyond algebra:** cumulative sums (discrete integration) of power → energy ([Khan calculus](https://www.khanacademy.org/math/calculus-1)); Monte Carlo over seeds ([Khan statistics](https://www.khanacademy.org/math/statistics-probability)).
- XML and URDF aren't used (the hexacopter model comes from Pegasus/PX4).

## 🎓 NVIDIA Cert Note
Composing terrain, lights and sensor prims into one stage maps to [NCP-OUSD OpenUSD Development Professional](https://www.nvidia.com/en-us/learn/certification/openusd-development-professional/) ($200). Free prep: [Learn OpenUSD](https://docs.nvidia.com/learn-openusd/latest/index.html). Second fit: DLI [NVIDIA Isaac for Accelerated Robotics (C-OV-05)](https://learn.nvidia.com/courses/course-detail?course_id=course-v1:DLI+C-OV-05+V1) — confirm price on the page.
