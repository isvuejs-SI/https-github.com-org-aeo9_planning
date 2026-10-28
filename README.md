# drone_pc

Planning and control stack for a solar-charging drone. The package is [`drone_pc`](drone_pc/README.md).

PX4 on the Pixhawk owns motors and failsafes. The Jetson companion only plans, supervises, and sends requests. Shared Python and C++17 tests live in `drone_pc/tests/` and do not need hardware.

```bash
./drone_pc/tests/run_tests.sh
```
