# Cursor rules for AEO-9 Planning & Control
- Deterministic: fixed random seeds, integer math where possible; Python and C++ twins must print identical output.
- Simulation only: no real flight code enabled until hardware starts in November. PX4 keeps its own failsafes; the supervisor only requests mode changes or waypoints.
- Each task gets a test before it counts as done; CI (./run_ci.sh) must pass, golden files updated only deliberately.
- Keep keys, footage, and logs out of Git.
- All numbers are illustrative placeholders, not real AEO-9 specs.
- Legal: drones are banned in Grand Canyon NP without authorization (36 CFR 1.5); planned routes stay outside the park.
