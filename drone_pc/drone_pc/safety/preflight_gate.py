from __future__ import annotations

from dataclasses import dataclass, field
from enum import IntEnum
from typing import List

from drone_pc.params import Params


class PreflightVerdict(IntEnum):
    GO = 0
    HOLD_GROUND = 1
    RTL = 2
    ABORT_MISSION = 3
    LAND_NOW = 4
    NO_GO = 5


def worse(a: PreflightVerdict, b: PreflightVerdict) -> PreflightVerdict:
    return a if int(a) >= int(b) else b


@dataclass
class PreflightInputs:
    wind_mps: float = 0.0
    cell_temp_c: float = 25.0
    soc_percent: float = 100.0
    geofence_ok: bool = True
    remote_id_ok: bool = True
    return_reserve_ok: bool = True
    airborne: bool = False


@dataclass
class PreflightResult:
    verdict: PreflightVerdict = PreflightVerdict.GO
    reasons: List[str] = field(default_factory=list)


class PreflightGate:
    def __init__(self, params: Params):
        self.params = params

    def evaluate(self, inp: PreflightInputs) -> PreflightResult:
        out = PreflightResult()

        if inp.wind_mps > self.params.preflight_max_wind_mps:
            out.verdict = worse(
                out.verdict, PreflightVerdict.RTL if inp.airborne else PreflightVerdict.NO_GO
            )
            out.reasons.append("wind_exceeds_limit")

        if inp.cell_temp_c > self.params.preflight_max_cell_temp_c:
            out.verdict = worse(
                out.verdict,
                PreflightVerdict.LAND_NOW if inp.airborne else PreflightVerdict.NO_GO,
            )
            out.reasons.append("cell_temp_high")
        elif inp.cell_temp_c < self.params.preflight_min_cell_temp_c:
            out.verdict = worse(
                out.verdict,
                PreflightVerdict.RTL if inp.airborne else PreflightVerdict.HOLD_GROUND,
            )
            out.reasons.append("cell_temp_low")

        if inp.soc_percent < self.params.preflight_min_soc_percent:
            out.verdict = worse(
                out.verdict,
                PreflightVerdict.RTL if inp.airborne else PreflightVerdict.HOLD_GROUND,
            )
            out.reasons.append("soc_below_min")

        if self.params.preflight_require_geofence_ok and not inp.geofence_ok:
            out.verdict = worse(
                out.verdict,
                PreflightVerdict.ABORT_MISSION if inp.airborne else PreflightVerdict.NO_GO,
            )
            out.reasons.append("geofence_violation")

        if self.params.preflight_require_remote_id and not inp.remote_id_ok:
            out.verdict = worse(
                out.verdict,
                PreflightVerdict.LAND_NOW if inp.airborne else PreflightVerdict.NO_GO,
            )
            out.reasons.append("remote_id_failed")

        if self.params.preflight_require_return_reserve and not inp.return_reserve_ok:
            out.verdict = worse(
                out.verdict,
                PreflightVerdict.RTL if inp.airborne else PreflightVerdict.HOLD_GROUND,
            )
            out.reasons.append("return_reserve_insufficient")

        return out
