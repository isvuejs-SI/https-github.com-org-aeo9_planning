# AEO-9 Planning & Control — Cursor task list (in order)

1. Repo skeleton: planner/, control/, sim/, tests/, config/aeo9.yaml, run_ci.sh
2. YAML config loader: wind limit, reserve energy, geofence radius, min AGL
3. Energy model: `E_next = min(C, max(0, E - draw + solar))`, launch gate `E >= E_need + E_reserve`
4. Mission planner: legs, per-leg energy estimate, day plan (launch, transit, filming, return, overnight)
5. Go/no-go gate: wind, battery, schedule, fence (fixed order) -> GO / WAIT / SKIP with reason
6. Failsafe state machine: NOMINAL / HOLD / RTL / LAND with latch
7. Geofence check: distance from home, breach handling
8. Animal escape: climb 35 m on close confident detection, count escapes, stop filming after 3
9. Low-battery return: return when remaining energy <= reserve + return cost
10. Gear & solar deploy sequence: stow before arming, deploy only after landing, refuse otherwise
11. PX4 adapter interface over MAVSDK (arm, takeoff, goto, hold, return, land) with a mock
12. ROS 2 nodes: subscribe BatteryState and Image, publish supervisor status, mocked camera trigger
13. Scenario tests: nominal, gust, low battery, geofence drift, repeated escapes, camera failure, Starlink outage
14. Golden-file Python/C++ diff in CI (fail on any diff)
15. Per-run CSV logging (time, state, energy, altitude)

## Rules
- Deterministic: fixed random seeds, integer math where possible; Python and C++ twins must print identical output.
- Simulation only: no real flight code enabled until hardware starts in November. PX4 keeps its own failsafes; the supervisor only requests mode changes or waypoints.
- Each task gets a test before it counts as done; CI (./run_ci.sh) must pass, golden files updated only deliberately.
- Keep keys, footage, and logs out of Git.
- All numbers are illustrative placeholders, not real AEO-9 specs.
- Legal: drones are banned in Grand Canyon NP without authorization (36 CFR 1.5); planned routes stay outside the park.

Existing reference code: failsafe/ (tasks 5–7, 9), mission_l17/ (3–4), day3_l18/ (3–5, 8, 13–15).
