"""A/B slots, boot-loop rollback, and the host CUDA stub."""

from pathlib import Path

from drone_ota.health import Observation, Platform, detect_platform, evaluate_health, load_health_config, tensorrt_status
from drone_ota.slots import SlotState, boot_staged, commit_pending, install_idle, note_health

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / "tests" / "fixtures"
HEALTH = load_health_config(str(ROOT / "config" / "ota_gate.yaml"))


def _reports():
    reports = {}
    name = None
    body = []
    for line in (FIXTURES / "health_reports.txt").read_text(encoding="utf-8").splitlines():
        if line.startswith("=== "):
            if name is not None:
                reports[name] = "\n".join(body) + "\n"
            name = line[4:]
            body = []
        else:
            body.append(line)
    if name is not None:
        reports[name] = "\n".join(body) + "\n"
    return reports


def _groups():
    groups = {}
    for line in (FIXTURES / "slot_trace.txt").read_text(encoding="utf-8").splitlines():
        group, rest = line.split(" ", 1)
        groups.setdefault(group, []).append(rest)
    return groups


def _base():
    return Observation(
        nodes=["failsafe", "logger", "telemetry"],
        heartbeat_age_s=1,
        sensor_age_s={"camera": 1, "mid360": 1},
        nvme_writable=True,
        time_offset_ms=0,
        boot_failures=0,
    )


def test_slot_trace_and_boot_loop():
    assert HEALTH.boot_loop_rollback_threshold == 3
    groups = _groups()
    rollback = SlotState()
    got = [install_idle(rollback, []), boot_staged(rollback)]
    got.append(note_health(rollback, False, HEALTH.boot_loop_rollback_threshold))
    got.append(note_health(rollback, False, HEALTH.boot_loop_rollback_threshold))
    got.append(note_health(rollback, False, HEALTH.boot_loop_rollback_threshold))
    assert got == groups["rollback"]
    assert rollback.active == "A"
    assert rollback.pending == ""
    assert rollback.boot_failures == 0

    denied = SlotState()
    assert install_idle(denied, ["battery_low"]) == groups["deny"][0]
    assert denied.active == "A" and denied.staged == ""

    commit = SlotState()
    committed = [
        install_idle(commit, []),
        boot_staged(commit),
        note_health(commit, True, HEALTH.boot_loop_rollback_threshold),
        commit_pending(commit),
        install_idle(commit, []),
    ]
    assert committed == groups["commit"]
    assert commit.active == "B"

    early = SlotState()
    assert install_idle(early, []) == "ALLOW wrote_slot=B keep_slot=A"
    assert boot_staged(early) == "BOOT pending=B active=A"
    assert note_health(early, False, 2) == "HEALTH_FAIL boot_loop=1 action=RETRY pending=B active=A"
    assert note_health(early, False, 2) == "HEALTH_FAIL boot_loop=2 action=ROLLBACK active=A"


def test_health_reports():
    assert HEALTH.max_time_offset_ms == 50
    assert HEALTH.health_window_s == 300
    assert HEALTH.tensorrt_required is False
    assert HEALTH.hardware_id == "jetson-orin"
    reports = _reports()
    base = _base()
    none = Platform()
    assert evaluate_health(base, HEALTH, none) == reports["all_ok"]

    missing = _base()
    missing.nodes = ["failsafe", "telemetry"]
    assert evaluate_health(missing, HEALTH, none) == reports["missing_logger"]

    disk = _base()
    disk.nvme_writable = False
    disk.boot_failures = 2
    assert evaluate_health(disk, HEALTH, none) == reports["rollback_nvme"]

    timed = _base()
    timed.time_offset_ms = 51
    assert evaluate_health(timed, HEALTH, none) == reports["time_fail"]

    heartbeat = _base()
    heartbeat.heartbeat_age_s = 4
    assert evaluate_health(heartbeat, HEALTH, none) == reports["heartbeat_fail"]

    sensor = _base()
    sensor.sensor_age_s = dict(sensor.sensor_age_s)
    sensor.sensor_age_s["camera"] = 4
    assert evaluate_health(sensor, HEALTH, none) == reports["sensor_fail"]

    both = _base()
    both.sensor_age_s = {"camera": 9}
    assert evaluate_health(both, HEALTH, none) == reports["both_sensors"]

    cuda = Platform(nvcc=True, gpu=True, cuda_linked=False)
    assert evaluate_health(base, HEALTH, cuda) == reports["cuda_unlinked"]

    required = load_health_config(str(ROOT / "config" / "ota_gate.yaml"))
    required.tensorrt_required = True
    assert evaluate_health(base, required, none) == reports["tensorrt_required"]


def test_host_cuda_is_stubbed():
    platform = detect_platform()
    assert platform.cuda_linked is False
    status = tensorrt_status(platform)
    assert status.startswith("SKIP")
    assert "no_nvcc_or_gpu" in status or "cuda_runtime_not_linked" in status
    assert status != "PASS"
