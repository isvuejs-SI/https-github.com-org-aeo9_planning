"""Preflight gate. The worst verdict wins.

Order from best to worst:
GO < HOLD_GROUND < RTL < ABORT_MISSION < LAND_NOW < NO_GO
"""

from __future__ import annotations

from drone_pc.common.geom import point_in_polygon
from drone_pc.common.params import Limits
from drone_pc.common.units import scale_milli, wh_to_micro
from drone_pc.planning.energy import available_micro, return_reserve_micro
from drone_pc.planning.global_planner import polygon_mm, polygons_mm

VERDICT_ORDER = ("GO", "HOLD_GROUND", "RTL", "ABORT_MISSION", "LAND_NOW", "NO_GO")
VERDICT_RANK = {name: index for index, name in enumerate(VERDICT_ORDER)}


def worst_verdict(names: list[str]) -> str:
    if not names:
        raise ValueError("no verdicts")
    return max(names, key=lambda name: VERDICT_RANK[name])


def _wind(wind_mps: float, limits: Limits) -> str:
    wind = scale_milli(wind_mps)
    if wind <= scale_milli(limits.wind_go_max_mps):
        return "GO"
    if wind <= scale_milli(limits.wind_hold_max_mps):
        return "HOLD_GROUND"
    return "NO_GO"


def _temperature(cell_temp_c: float, limits: Limits) -> str:
    temp = scale_milli(cell_temp_c)
    if temp <= scale_milli(limits.temp_no_go_low_c) or temp >= scale_milli(limits.temp_no_go_high_c):
        return "NO_GO"
    if temp <= scale_milli(limits.temp_land_now_low_c) or temp >= scale_milli(limits.temp_land_now_high_c):
        return "LAND_NOW"
    if temp < scale_milli(limits.temp_hold_low_c) or temp > scale_milli(limits.temp_hold_high_c):
        return "HOLD_GROUND"
    return "GO"


def _soc(soc: float, limits: Limits) -> str:
    level = scale_milli(soc)
    if level >= scale_milli(limits.soc_go_min):
        return "GO"
    if level >= scale_milli(limits.soc_hold_min):
        return "HOLD_GROUND"
    if level >= scale_milli(limits.soc_rtl_min):
        return "RTL"
    return "NO_GO"


def _geofence(x_m: float, y_m: float, alt_m: float, limits: Limits) -> str:
    x_mm = scale_milli(x_m)
    y_mm = scale_milli(y_m)
    alt = scale_milli(alt_m)
    fence = polygon_mm(limits.geofence)
    no_fly = polygons_mm(limits.no_fly)
    if fence and not point_in_polygon(x_mm, y_mm, fence):
        return "NO_GO"
    for polygon in no_fly:
        if point_in_polygon(x_mm, y_mm, polygon):
            return "NO_GO"
    if alt > scale_milli(limits.alt_max_m):
        return "NO_GO"
    if alt < scale_milli(limits.alt_min_m):
        return "HOLD_GROUND"
    return "GO"


def _remote_id(status: str) -> str:
    if status == "ok":
        return "GO"
    if status == "degraded":
        return "HOLD_GROUND"
    return "NO_GO"


def _reserve(soc: float, distance_km: float, mission_wh: float, limits: Limits) -> str:
    available = available_micro(soc, limits.pack_capacity_wh)
    reserve = return_reserve_micro(
        distance_km,
        limits.wh_per_km,
        limits.return_distance_factor,
        limits.pack_capacity_wh,
        limits.pack_reserve_fraction,
    )
    mission = wh_to_micro(mission_wh)
    if available >= reserve + mission:
        return "GO"
    if available >= reserve:
        return "HOLD_GROUND"
    return "NO_GO"


def _abort(operator_abort: bool) -> str:
    if operator_abort:
        return "ABORT_MISSION"
    return "GO"


def evaluate_preflight(
    limits: Limits,
    wind_mps: float,
    cell_temp_c: float,
    soc: float,
    x_m: float,
    y_m: float,
    alt_m: float,
    remote_id: str,
    distance_km: float,
    mission_wh: float,
    operator_abort: bool,
) -> dict:
    checks = {
        "abort_switch": _abort(operator_abort),
        "cell_temp": _temperature(cell_temp_c, limits),
        "geofence": _geofence(x_m, y_m, alt_m, limits),
        "remote_id": _remote_id(remote_id),
        "return_reserve": _reserve(soc, distance_km, mission_wh, limits),
        "soc": _soc(soc, limits),
        "wind": _wind(wind_mps, limits),
    }
    verdict = worst_verdict(list(checks.values()))
    if verdict == "GO":
        reason = "all_clear"
    else:
        reason = "+".join(sorted(name for name, value in checks.items() if value == verdict))
    return {"verdict": verdict, "reason": reason, "checks": checks}
