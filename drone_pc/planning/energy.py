"""Return reserve, overnight solar-versus-return, and a backup landing spot.

reserve_Wh = distance_km * Wh_per_km * distance_factor + pack_fraction * pack_Wh
Factors are arguments supplied from YAML by the caller.
"""

from __future__ import annotations

from drone_pc.common.geom import point_in_polygon
from drone_pc.common.units import div_round, hypot_mm, scale_milli, wh_to_micro
from drone_pc.planning.global_planner import polygon_mm, polygons_mm


def return_reserve_from_milli_km(
    distance_milli_km: int,
    wh_per_km: float,
    distance_factor: float,
    pack_wh: float,
    pack_fraction: float,
) -> int:
    wh_milli = scale_milli(wh_per_km)
    factor_milli = scale_milli(distance_factor)
    fraction_milli = scale_milli(pack_fraction)
    pack_micro = wh_to_micro(pack_wh)
    term_distance = div_round(int(distance_milli_km) * wh_milli * factor_milli, 1000)
    term_pack = div_round(fraction_milli * pack_micro, 1000)
    return term_distance + term_pack


def return_reserve_micro(
    distance_km: float,
    wh_per_km: float,
    distance_factor: float,
    pack_wh: float,
    pack_fraction: float,
) -> int:
    return return_reserve_from_milli_km(
        scale_milli(distance_km), wh_per_km, distance_factor, pack_wh, pack_fraction
    )


def return_reserve_micro_from_mm(
    distance_mm: int,
    wh_per_km: float,
    distance_factor: float,
    pack_wh: float,
    pack_fraction: float,
) -> int:
    """Horizontal distance in millimeters. One milli-kilometer equals one meter."""
    return return_reserve_from_milli_km(
        div_round(int(distance_mm), 1000),
        wh_per_km,
        distance_factor,
        pack_wh,
        pack_fraction,
    )


def available_micro(soc: float, pack_wh: float) -> int:
    return div_round(scale_milli(soc) * wh_to_micro(pack_wh), 1000)


def overnight_decision(
    soc: float,
    pack_wh: float,
    solar_wh: float,
    distance_km: float,
    mission_wh: float,
    wh_per_km: float,
    distance_factor: float,
    pack_fraction: float,
    airborne: bool,
) -> str:
    """Parked: stay and charge when morning energy covers mission plus reserve.

    If the pack can still get home but solar will not recover the mission,
    return now. If it cannot get home, stay and charge.

    Airborne: return now when current energy is below the reserve or below
    mission plus reserve. Otherwise continue. A flying vehicle cannot charge.
    """
    available = available_micro(soc, pack_wh)
    reserve = return_reserve_micro(
        distance_km, wh_per_km, distance_factor, pack_wh, pack_fraction
    )
    mission = wh_to_micro(mission_wh)
    solar = wh_to_micro(solar_wh)
    pack = wh_to_micro(pack_wh)
    morning = pack if available + solar > pack else available + solar
    need = mission + reserve
    if airborne:
        if available < reserve or available < need:
            return "RETURN_NOW"
        return "CONTINUE"
    if morning >= need:
        return "STAY_CHARGE"
    if available >= reserve:
        return "RETURN_NOW"
    return "STAY_CHARGE"


def _candidate_mm(candidate: dict) -> tuple[int, int, int]:
    return (
        scale_milli(float(candidate["x"])),
        scale_milli(float(candidate["y"])),
        scale_milli(float(candidate["z"])),
    )


def plan_backup(
    origin_m,
    candidates,
    blocked_ids,
    geofence,
    no_fly,
    soc: float,
    pack_wh: float,
    wh_per_km: float,
    distance_factor: float,
    pack_fraction: float,
) -> dict | None:
    """Nearest reachable site inside the fence and outside no-fly zones.

    Tie-break is smaller horizontal distance, then smaller id. When every
    feasible site is beyond the return reserve, the nearest legal site is
    still returned with energy_shortfall set.
    """
    fence_mm = polygon_mm(geofence)
    no_fly_mm = polygons_mm(no_fly)
    origin = (scale_milli(float(origin_m[0])), scale_milli(float(origin_m[1])))
    available = available_micro(soc, pack_wh)
    blocked = set(int(item) for item in blocked_ids)
    legal = []
    reachable = []
    for candidate in candidates:
        ident = int(candidate["id"])
        if ident in blocked:
            continue
        x_mm, y_mm, z_mm = _candidate_mm(candidate)
        if fence_mm and not point_in_polygon(x_mm, y_mm, fence_mm):
            continue
        if any(point_in_polygon(x_mm, y_mm, polygon) for polygon in no_fly_mm):
            continue
        dx = x_mm - origin[0]
        dy = y_mm - origin[1]
        dist_sq = dx * dx + dy * dy
        dist_mm = hypot_mm(dx, dy, 0)
        reserve = return_reserve_micro_from_mm(
            dist_mm, wh_per_km, distance_factor, pack_wh, pack_fraction
        )
        record = {
            "id": ident,
            "x_mm": x_mm,
            "y_mm": y_mm,
            "z_mm": z_mm,
            "distance_mm": dist_mm,
            "reserve_micro_wh": reserve,
            "dist_sq": dist_sq,
        }
        legal.append(record)
        if reserve <= available:
            reachable.append(record)
    pool = reachable if reachable else legal
    if not pool:
        return None
    pool.sort(key=lambda item: (item["dist_sq"], item["id"]))
    chosen = pool[0]
    return {
        "id": chosen["id"],
        "x_mm": chosen["x_mm"],
        "y_mm": chosen["y_mm"],
        "z_mm": chosen["z_mm"],
        "distance_mm": chosen["distance_mm"],
        "reserve_micro_wh": chosen["reserve_micro_wh"],
        "energy_shortfall": not reachable,
    }
