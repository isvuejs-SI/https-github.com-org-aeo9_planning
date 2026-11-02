# AEO-9 UI (Flutter)

Operator interface for the AEO-9 solar-perch hexacopter. Targets **iOS** and **macOS** (Windows later). Simulation-first: the UI requests HOLD / RTL / LAND; Pixhawk keeps failsafes.

## Screens (5 total — this PR is screen 1)

| # | Screen | Role |
|---|--------|------|
| 1 | **Mission** | Flight picture, map, go/no-go, autonomy requests (features 1–6 + glanceables) |
| 2 | Live | Video / thermal / gimbal / record |
| 3 | Aircraft | Power, solar, sensors, airframe |
| 4 | Link | Starlink, radio, alerts |
| 5 | Plan | Day plan, preflight, geofence legal notes |

The **32 product features** live inside these five screens, not as thirty-two pages.

## Run

```bash
cd aeo9_ui
flutter pub get
flutter run -d chrome          # quick display
flutter run -d macos           # desktop window
flutter run -d ios             # device / simulator
```

```bash
flutter test
```

## Safety

- `SIM ONLY` banner while mocks are active.
- No actuator or arm path in this UI layer.
- Staging home label: `tusayan_staging` (outside Grand Canyon NP).
