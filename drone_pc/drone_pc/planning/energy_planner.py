from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import List, Optional

from drone_pc.params import Params
from drone_pc.planning.occupancy_map import OccupancyMap, Point3


@dataclass
class EnergyPlanInput:
    distance_to_home_km: float = 0.0
    soc_percent: float = 100.0
    remaining_mission_km: float = 0.0
    overnight: bool = False
    current_position: Point3 = field(default_factory=Point3)
    candidate_backup_sites: List[Point3] = field(default_factory=list)


@dataclass
class EnergyPlanResult:
    return_reserve_wh: float = 0.0
    available_wh: float = 0.0
    return_reserve_ok: bool = False
    stay_overnight: bool = False
    backup_selected: bool = False
    backup_site: Point3 = field(default_factory=Point3)
    reason: str = ""


class EnergyPlanner:
    def __init__(self, params: Params):
        self.params = params

    def compute_return_reserve_wh(self, distance_km: float) -> float:
        return (
            distance_km
            * self.params.energy_wh_per_km
            * self.params.energy_reserve_distance_factor
            + self.params.energy_pack_reserve_fraction * self.params.energy_pack_capacity_wh
        )

    def evaluate(
        self, inp: EnergyPlanInput, mmap: Optional[OccupancyMap] = None
    ) -> EnergyPlanResult:
        out = EnergyPlanResult()
        out.return_reserve_wh = self.compute_return_reserve_wh(inp.distance_to_home_km)
        out.available_wh = (inp.soc_percent / 100.0) * self.params.energy_pack_capacity_wh
        mission_wh = (
            inp.remaining_mission_km
            * self.params.energy_wh_per_km
            * self.params.energy_reserve_distance_factor
        )
        out.return_reserve_ok = out.available_wh >= (out.return_reserve_wh + mission_wh)

        if inp.overnight:
            deficit = max(0.0, out.return_reserve_wh - out.available_wh)
            out.stay_overnight = self.params.energy_overnight_solar_wh_estimate >= deficit
            out.reason = "overnight_solar_ok" if out.stay_overnight else "overnight_return_preferred"
        else:
            out.reason = "energy_ok" if out.return_reserve_ok else "energy_short"

        best = float("inf")
        for site in inp.candidate_backup_sites:
            if mmap is not None:
                if (
                    not mmap.inside_geofence(site)
                    or mmap.in_nofly(site)
                    or mmap.occupied_world(site)
                ):
                    continue
            d = math.hypot(
                inp.current_position.x - site.x, inp.current_position.y - site.y
            )
            if d < self.params.energy_min_backup_landing_clearance_m:
                continue
            if d < best:
                best = d
                out.backup_site = site
                out.backup_selected = True

        if not out.backup_selected and out.reason == "energy_ok":
            out.reason = "energy_ok_no_backup"
        elif out.backup_selected and out.reason == "energy_ok":
            out.reason = "energy_ok_backup_selected"
        return out
