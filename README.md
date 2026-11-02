# AEO-9 planning

Simulation-first software for the AEO-9 solar-perch hexacopter.

## Packages

| Path | Role |
|------|------|
| [`aeo9_ui/`](aeo9_ui/README.md) | Flutter operator UI (iOS + macOS). Screen 1 Mission shipped; screens 2–5 next. |

PX4 on the flight controller owns motors and failsafes. Supervisor and UI only request mode changes or waypoints.
