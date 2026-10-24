"""Deterministic energy model: E_next = min(C, max(0, E - draw + solar))."""

from __future__ import annotations

from dataclasses import dataclass


@dataclass
class EnergyState:
    energy_mwh: int
    capacity_mwh: int

    def clamp(self) -> "EnergyState":
        e = self.energy_mwh
        if e < 0:
            e = 0
        if e > self.capacity_mwh:
            e = self.capacity_mwh
        return EnergyState(energy_mwh=e, capacity_mwh=self.capacity_mwh)


def apply_energy_step(
    energy_mwh: int,
    capacity_mwh: int,
    draw_mwh: int,
    solar_mwh: int,
) -> int:
    """E_next = min(C, max(0, E - draw + solar)) using integer math."""
    next_e = energy_mwh - draw_mwh + solar_mwh
    if next_e < 0:
        next_e = 0
    if next_e > capacity_mwh:
        next_e = capacity_mwh
    return next_e


def launch_gate(
    energy_mwh: int,
    energy_need_mwh: int,
    reserve_mwh: int,
) -> tuple[bool, str]:
    """Allow launch only when E >= E_need + E_reserve."""
    threshold = energy_need_mwh + reserve_mwh
    if energy_mwh >= threshold:
        return True, f"LAUNCH_OK E={energy_mwh} need+reserve={threshold}"
    return False, f"LAUNCH_BLOCKED E={energy_mwh} need+reserve={threshold}"
