"""Load AEO-9 flight limits from YAML into integer fields."""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

import yaml


@dataclass(frozen=True)
class Waypoint:
    name: str
    north_cm: int
    east_cm: int
    alt_cm: int
    leg: str


@dataclass(frozen=True)
class Aeo9Config:
    seed: int
    sim_only: bool
    max_wind_cm_s: int
    reserve_energy_mwh: int
    geofence_radius_cm: int
    min_altitude_agl_cm: int
    capacity_mwh: int
    hover_draw_mwh_per_s: int
    transit_draw_mwh_per_s: int
    film_draw_mwh_per_s: int
    solar_charge_mwh_per_s: int
    animal_escape_climb_cm: int
    animal_escape_stop_after: int
    filming_leg_mwh: int
    return_leg_mwh: int
    home_name: str
    home_lat_e7: int
    home_lon_e7: int
    home_alt_cm: int
    waypoints: tuple[Waypoint, ...] = field(default_factory=tuple)

    def dump_lines(self) -> list[str]:
        lines = [
            f"seed={self.seed}",
            f"sim_only={1 if self.sim_only else 0}",
            f"max_wind_cm_s={self.max_wind_cm_s}",
            f"reserve_energy_mwh={self.reserve_energy_mwh}",
            f"geofence_radius_cm={self.geofence_radius_cm}",
            f"min_altitude_agl_cm={self.min_altitude_agl_cm}",
            f"capacity_mwh={self.capacity_mwh}",
            f"home={self.home_name}",
            f"home_lat_e7={self.home_lat_e7}",
            f"home_lon_e7={self.home_lon_e7}",
            f"waypoints={len(self.waypoints)}",
        ]
        return lines


def _require_int(node: dict[str, Any], key: str) -> int:
    if key not in node:
        raise KeyError(f"missing config key: {key}")
    value = node[key]
    if isinstance(value, bool) or not isinstance(value, int):
        raise TypeError(f"config key {key} must be int, got {type(value).__name__}")
    return value


def load_config(path: str | Path) -> Aeo9Config:
    cfg_path = Path(path)
    raw = yaml.safe_load(cfg_path.read_text(encoding="utf-8"))
    if not isinstance(raw, dict):
        raise ValueError("aeo9 config root must be a mapping")

    limits = raw["limits"]
    energy = raw["energy"]
    mission = raw["mission"]
    home = raw["home"]
    wps_raw = raw.get("waypoints") or []

    waypoints: list[Waypoint] = []
    for item in wps_raw:
        waypoints.append(
            Waypoint(
                name=str(item["name"]),
                north_cm=int(item["north_cm"]),
                east_cm=int(item["east_cm"]),
                alt_cm=int(item["alt_cm"]),
                leg=str(item["leg"]),
            )
        )

    return Aeo9Config(
        seed=_require_int(raw, "seed"),
        sim_only=bool(raw["sim_only"]),
        max_wind_cm_s=_require_int(limits, "max_wind_cm_s"),
        reserve_energy_mwh=_require_int(limits, "reserve_energy_mwh"),
        geofence_radius_cm=_require_int(limits, "geofence_radius_cm"),
        min_altitude_agl_cm=_require_int(limits, "min_altitude_agl_cm"),
        capacity_mwh=_require_int(energy, "capacity_mwh"),
        hover_draw_mwh_per_s=_require_int(energy, "hover_draw_mwh_per_s"),
        transit_draw_mwh_per_s=_require_int(energy, "transit_draw_mwh_per_s"),
        film_draw_mwh_per_s=_require_int(energy, "film_draw_mwh_per_s"),
        solar_charge_mwh_per_s=_require_int(energy, "solar_charge_mwh_per_s"),
        animal_escape_climb_cm=_require_int(mission, "animal_escape_climb_cm"),
        animal_escape_stop_after=_require_int(mission, "animal_escape_stop_after"),
        filming_leg_mwh=_require_int(mission, "filming_leg_mwh"),
        return_leg_mwh=_require_int(mission, "return_leg_mwh"),
        home_name=str(home["name"]),
        home_lat_e7=_require_int(home, "lat_e7"),
        home_lon_e7=_require_int(home, "lon_e7"),
        home_alt_cm=_require_int(home, "alt_cm"),
        waypoints=tuple(waypoints),
    )
