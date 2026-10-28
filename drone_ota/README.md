# drone_ota

C++17 library and Python module for companion-computer updates. PX4 on the Pixhawk is out of scope: the Jetson may read MAVLink and send requests, and PX4 keeps the failsafes. Nothing here flashes the flight controller.

## What installs where

| Layer | Mechanism in this repo |
|---|---|
| ROS nodes, configs, detection engines | Signed app bundle. Ed25519 over the canonical manifest bytes, then SHA-256 of each listed file. Public key only on the device. Install goes to the idle A/B slot, then boot, then commit or roll back. |
| JetPack / L4T (kernel, bootloader, CUDA) | NVIDIA A/B rootfs. Not vendored here. |
| Pixhawk firmware | Manual flash, props off, after a human release check. |

Downloading a bundle is not gated. Installing, switching slots, and rebooting are gated.

The gate allows an update only when the vehicle is landed, disarmed, and charging, with battery, disk, and temperature inside the YAML limits, no failsafe active, and a valid signature. It refuses an update in flight.

## Canonical manifest

UTF-8, one trailing newline, this exact layout. The signature covers these bytes and nothing else:

```
DRONEOTA1
version=<version>
hardware=<id>
min_compatible=<version>
max_compatible=<version>
<sha256>  <size> <path>
```

File lines are sorted by path. The lesson-9 mission file stays in its one-line form (`<sha256> <file> <version>`). That file is signed the same way: Ed25519 over the manifest bytes, then the SHA-256 of the plan file.

Tests generate an ephemeral keypair in memory. Do not commit a private key.

## Boot health

After a slot switch the health check looks for required nodes, a MAVLink heartbeat, sensors, a writable NVMe, and time sync. TensorRT is a stub on a host build: the report says `SKIP no_nvcc_or_gpu` or `SKIP cuda_runtime_not_linked`. It does not pretend an engine loaded. The boot-loop counter rolls back at `health.boot_loop_rollback_threshold` in `config/ota_gate.yaml`.

## Tests

From the repo root:

```bash
./scripts/run_host_tests.sh
```

GoogleTest covers the C++ library. pytest covers the Python module. Both read the same fixture files.
