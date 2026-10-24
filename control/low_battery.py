"""Low-battery: return when remaining energy <= reserve + return cost."""

from __future__ import annotations


def should_return_low_battery(
    energy_mwh: int,
    reserve_mwh: int,
    return_cost_mwh: int,
) -> tuple[bool, str]:
    threshold = reserve_mwh + return_cost_mwh
    if energy_mwh <= threshold:
        return True, f"LOW_BATTERY_RTL E={energy_mwh}<={threshold}"
    return False, f"BATTERY_OK E={energy_mwh}>{threshold}"
