"""Summary packet. The byte budget lives in config/telemetry.yaml."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from drone_ota.yaml_subset import load_yaml

CONFIG_PATH = Path(__file__).resolve().parent / "config" / "telemetry.yaml"


@dataclass
class TelemetryConfig:
    max_bytes_per_sec: int
    publish_hz: int
    temp_warn_c: float
    temp_fail_c: float
    nvme_free_warn_pct: float
    nvme_wear_warn_pct: float
    max_field_len: int


def load_telemetry_config(path: str | None = None) -> TelemetryConfig:
    data = load_yaml(str(path or CONFIG_PATH))["telemetry"]
    return TelemetryConfig(
        max_bytes_per_sec=int(data["max_bytes_per_sec"]),
        publish_hz=int(data["publish_hz"]),
        temp_warn_c=float(data["temp_warn_c"]),
        temp_fail_c=float(data["temp_fail_c"]),
        nvme_free_warn_pct=float(data["nvme_free_warn_pct"]),
        nvme_wear_warn_pct=float(data["nvme_wear_warn_pct"]),
        max_field_len=int(data["max_field_len"]),
    )


def jetson_level(temp_c: float, nvme_free_pct: float, wear_pct: float, config: TelemetryConfig) -> str:
    if temp_c > config.temp_fail_c or wear_pct > config.nvme_wear_warn_pct:
        return "FAIL"
    if temp_c > config.temp_warn_c or nvme_free_pct < config.nvme_free_warn_pct:
        return "WARN"
    return "OK"


def thermal_level(temp_c: float, config: TelemetryConfig) -> str:
    if temp_c > config.temp_fail_c:
        return "HOT"
    if temp_c > config.temp_warn_c:
        return "WARM"
    return "OK"


def _clip(text: str, limit: int) -> str:
    return text[:limit]


def _battery(battery_pct: float) -> str:
    tenths = int(round(battery_pct * 10))
    sign = "-" if tenths < 0 else ""
    tenths = abs(tenths)
    return f"{sign}{tenths // 10}.{tenths % 10}"


def encode_summary(
    mode: str,
    battery_pct: float,
    temp_c: float,
    nvme_free_pct: float,
    wear_pct: float,
    detection: bool,
    failsafe: str,
    config: TelemetryConfig,
) -> str:
    jetson = jetson_level(temp_c, nvme_free_pct, wear_pct, config)
    thermal = thermal_level(temp_c, config)
    mode_s = _clip(mode, config.max_field_len)
    fail_s = _clip(failsafe, config.max_field_len)
    det = 1 if detection else 0
    return (
        f"mode={mode_s} battery_pct={_battery(battery_pct)} jetson={jetson} "
        f"thermal={thermal} detection={det} failsafe={fail_s}\n"
    )


def within_budget(nbytes: int, hz: int, max_bps: int) -> bool:
    return nbytes * hz < max_bps
