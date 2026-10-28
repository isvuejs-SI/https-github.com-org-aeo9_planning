# https-github.com-org-aeo9_planning

Companion-computer DevOps for the solar-charging drone: logging, telemetry, and OTA. PX4 on the Pixhawk owns failsafes. These packages never auto-update Pixhawk firmware. MAVLink is a request and telemetry interface only.

| Package | Role |
|---|---|
| [`drone_ota`](drone_ota/README.md) | Precondition gate, Ed25519 signed bundles, A/B slot state, boot health |
| [`drone_logging`](drone_logging) | JSON-lines events, rosbag2 MCAP launch, landing upload queue |
| [`drone_telemetry`](drone_telemetry) | Summary packet held under 1 kB/s |

Update split (no Mender, RAUC, or SWUpdate in this tree):

- **ROS app bundles** on the Jetson are signed (Ed25519 over the canonical manifest, then SHA-256 of each file) and installed only through the gate, into the idle A/B slot.
- **JetPack / L4T** stays on NVIDIA's own A/B rootfs path. This repo does not vendor that updater.
- **Pixhawk / PX4 firmware** is manual. A human flashes it with props off after SITL and bench checks.

Host tests (no drone, Pixhawk, Starlink, Jetson, or GPU):

```bash
./scripts/run_host_tests.sh
```

Thresholds are examples copied into YAML from the design notes. They are not measured limits. The gate reads `drone_ota/config/ota_gate.yaml` only.
