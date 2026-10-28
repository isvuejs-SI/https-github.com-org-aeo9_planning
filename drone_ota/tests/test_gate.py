"""Gate cases shared with the C++ tests."""

from pathlib import Path

import pytest

from drone_ota.gate import (
    check_gate,
    format_gate_line,
    format_lesson_ota,
    gate_config_from_yaml,
    load_gate_config,
)
from drone_ota.gate import GateState

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / "tests" / "fixtures"
CONFIG = load_gate_config(str(ROOT / "config" / "ota_gate.yaml"))


def _state(fields):
    return GateState(
        armed=fields[0] == "1",
        landed=fields[1] == "1",
        batt_pct=float(fields[2]),
        charging=fields[3] == "1",
        nvme_free_gb=float(fields[4]),
        jetson_c=float(fields[5]),
        failsafe_active=fields[6] == "1",
        sig_ok=fields[7] == "1",
    )


def load_cases():
    rows = {}
    for line in (FIXTURES / "gate_cases.txt").read_text(encoding="utf-8").splitlines():
        if not line or line.startswith("#"):
            continue
        fields = line.split("|")
        reasons = [] if fields[9] == "" else fields[9].split(",")
        rows[fields[0]] = (_state(fields[1:9]), reasons)
    return rows


CASES = load_cases()


def test_shipped_examples():
    assert CONFIG.min_batt_pct == 60.0
    assert CONFIG.min_nvme_free_gb == 20.0
    assert CONFIG.max_jetson_c == 70.0


@pytest.mark.parametrize("name", sorted(CASES))
def test_fixture_case(name):
    state, reasons = CASES[name]
    assert check_gate(state, CONFIG) == reasons


def test_allow():
    state, reasons = CASES["allow"]
    assert check_gate(state, CONFIG) == []
    assert reasons == []


def test_in_flight():
    state, _reasons = CASES["in_flight"]
    assert check_gate(state, CONFIG) == ["armed", "not_landed"]


def test_low_battery():
    state, _reasons = CASES["low_battery"]
    assert check_gate(state, CONFIG) == ["battery_low"]


def test_bad_sig_and_disk():
    state, _reasons = CASES["bad_sig_and_disk"]
    assert check_gate(state, CONFIG) == ["nvme_full", "bad_signature"]


def test_failsafe_blocks():
    state, _reasons = CASES["failsafe_blocks"]
    assert check_gate(state, CONFIG) == ["failsafe_active"]


def test_design_doc_stdout():
    lines = []
    for line in (FIXTURES / "gate_stdout.txt").read_text(encoding="utf-8").splitlines():
        name = line.split(": ", 1)[0]
        state, _reasons = CASES[name]
        lines.append(format_gate_line(name, check_gate(state, CONFIG)))
    assert "\n".join(lines) + "\n" == (FIXTURES / "gate_stdout.txt").read_text(encoding="utf-8")


def test_lesson_ota_stdout():
    states = {}
    for line in (FIXTURES / "lesson_ota.txt").read_text(encoding="utf-8").splitlines():
        fields = line.split("|")
        states[fields[0]] = _state(fields[1:9])
    got = []
    for line in (FIXTURES / "lesson_ota_stdout.txt").read_text(encoding="utf-8").splitlines():
        name = line.split(": ", 1)[0]
        got.append(format_lesson_ota(name, check_gate(states[name], CONFIG)))
    assert "\n".join(got) + "\n" == (FIXTURES / "lesson_ota_stdout.txt").read_text(encoding="utf-8")


def test_thresholds_come_from_yaml():
    loose = gate_config_from_yaml(
        "gate:\n  min_batt_pct: 10.0\n  min_nvme_free_gb: 1.0\n  max_jetson_c: 90.0\n"
    )
    state = GateState(False, True, 41, True, 140, 48, False, True)
    assert check_gate(state, CONFIG) == ["battery_low"]
    assert check_gate(state, loose) == []
