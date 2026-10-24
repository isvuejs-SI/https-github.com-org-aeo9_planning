"""Go/no-go gate: wind, battery, schedule, fence in fixed order."""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum


class GateResult(str, Enum):
    GO = "GO"
    WAIT = "WAIT"
    SKIP = "SKIP"


@dataclass(frozen=True)
class GateDecision:
    result: GateResult
    reason: str

    def dump_line(self) -> str:
        return f"go_nogo={self.result.value} reason={self.reason}"


def evaluate_go_nogo(
    *,
    wind_cm_s: int,
    max_wind_cm_s: int,
    energy_mwh: int,
    energy_need_mwh: int,
    reserve_mwh: int,
    schedule_ok: bool,
    inside_fence: bool,
) -> GateDecision:
    """Fixed check order: wind → battery → schedule → fence."""
    if wind_cm_s > max_wind_cm_s:
        return GateDecision(GateResult.WAIT, f"wind={wind_cm_s}>{max_wind_cm_s}")
    threshold = energy_need_mwh + reserve_mwh
    if energy_mwh < threshold:
        return GateDecision(GateResult.WAIT, f"battery E={energy_mwh}<{threshold}")
    if not schedule_ok:
        return GateDecision(GateResult.SKIP, "schedule_closed")
    if not inside_fence:
        return GateDecision(GateResult.SKIP, "outside_fence")
    return GateDecision(GateResult.GO, "all_checks_passed")
