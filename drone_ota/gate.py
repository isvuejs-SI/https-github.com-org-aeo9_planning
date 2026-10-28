"""OTA precondition gate. Thresholds come from config/ota_gate.yaml."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from drone_ota.yaml_subset import load_yaml, parse_yaml

CONFIG_PATH = Path(__file__).resolve().parent / "config" / "ota_gate.yaml"


@dataclass
class GateConfig:
    min_batt_pct: float
    min_nvme_free_gb: float
    max_jetson_c: float


@dataclass
class GateState:
    armed: bool
    landed: bool
    batt_pct: float
    charging: bool
    nvme_free_gb: float
    jetson_c: float
    failsafe_active: bool
    sig_ok: bool


def _config_from(data) -> GateConfig:
    gate = data["gate"]
    return GateConfig(
        min_batt_pct=float(gate["min_batt_pct"]),
        min_nvme_free_gb=float(gate["min_nvme_free_gb"]),
        max_jetson_c=float(gate["max_jetson_c"]),
    )


def load_gate_config(path: str | None = None) -> GateConfig:
    return _config_from(load_yaml(str(path or CONFIG_PATH)))


def gate_config_from_yaml(text: str) -> GateConfig:
    return _config_from(parse_yaml(text))


def check_gate(state: GateState | dict, config: GateConfig) -> list[str]:
    if isinstance(state, dict):
        state = GateState(**state)
    reasons: list[str] = []
    if state.armed:
        reasons.append("armed")
    if not state.landed:
        reasons.append("not_landed")
    if state.batt_pct < config.min_batt_pct:
        reasons.append("battery_low")
    if not state.charging:
        reasons.append("not_charging")
    if state.nvme_free_gb < config.min_nvme_free_gb:
        reasons.append("nvme_full")
    if state.jetson_c > config.max_jetson_c:
        reasons.append("too_hot")
    if state.failsafe_active:
        reasons.append("failsafe_active")
    if not state.sig_ok:
        reasons.append("bad_signature")
    return reasons


def format_gate_line(name: str, reasons: list[str]) -> str:
    if not reasons:
        return f"{name}: ALLOW"
    return f"{name}: DENY {','.join(reasons)}"


def format_lesson_ota(name: str, reasons: list[str]) -> str:
    if not reasons:
        return f"{name}: ALLOW (keep slot B for rollback)"
    return f"{name}: DENY {','.join(reasons)}"
