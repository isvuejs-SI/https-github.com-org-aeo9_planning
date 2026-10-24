"""Mission planner: waypoint legs, per-leg energy, day plan phases."""

from __future__ import annotations

from dataclasses import dataclass

from planner.config_loader import Aeo9Config, Waypoint


PHASE_ORDER = ("launch", "transit", "filming", "return", "overnight")


@dataclass(frozen=True)
class LegEstimate:
    name: str
    leg: str
    north_cm: int
    east_cm: int
    alt_cm: int
    energy_mwh: int


@dataclass(frozen=True)
class DayPlan:
    phases: tuple[str, ...]
    legs: tuple[LegEstimate, ...]
    total_energy_mwh: int

    def dump_lines(self) -> list[str]:
        lines = [f"phases={','.join(self.phases)}", f"leg_count={len(self.legs)}"]
        for leg in self.legs:
            lines.append(
                f"leg name={leg.name} type={leg.leg} "
                f"n={leg.north_cm} e={leg.east_cm} alt={leg.alt_cm} mwh={leg.energy_mwh}"
            )
        lines.append(f"total_energy_mwh={self.total_energy_mwh}")
        return lines


def _distance_cm(a: Waypoint, b: Waypoint) -> int:
    dn = b.north_cm - a.north_cm
    de = b.east_cm - a.east_cm
    # Integer approximation: Chebyshev + half Manhattan residual (deterministic).
    abs_n = abs(dn)
    abs_e = abs(de)
    if abs_n > abs_e:
        return abs_n + abs_e // 2
    return abs_e + abs_n // 2


def _leg_energy_mwh(cfg: Aeo9Config, leg_type: str, dist_cm: int) -> int:
    # 1 mWh per 100 cm of path plus profile baseline.
    path = dist_cm // 100
    if leg_type == "launch":
        return cfg.hover_draw_mwh_per_s * 10 + path
    if leg_type == "transit":
        return cfg.transit_draw_mwh_per_s * 20 + path
    if leg_type == "filming":
        return cfg.filming_leg_mwh + path
    if leg_type == "return":
        return cfg.return_leg_mwh + path
    if leg_type == "overnight":
        return 0
    return path


def plan_day(cfg: Aeo9Config) -> DayPlan:
    if not cfg.waypoints:
        return DayPlan(phases=PHASE_ORDER, legs=(), total_energy_mwh=0)

    legs: list[LegEstimate] = []
    prev = cfg.waypoints[0]
    for wp in cfg.waypoints:
        dist = _distance_cm(prev, wp)
        energy = _leg_energy_mwh(cfg, wp.leg, dist)
        legs.append(
            LegEstimate(
                name=wp.name,
                leg=wp.leg,
                north_cm=wp.north_cm,
                east_cm=wp.east_cm,
                alt_cm=wp.alt_cm,
                energy_mwh=energy,
            )
        )
        prev = wp

    total = sum(leg.energy_mwh for leg in legs)
    return DayPlan(phases=PHASE_ORDER, legs=tuple(legs), total_energy_mwh=total)
