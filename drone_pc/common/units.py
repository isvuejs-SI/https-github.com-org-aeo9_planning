"""Integer scaling shared by every module.

Flight limits live in config YAML. The constants here convert units only:
millimeters, milliseconds, and micro-watt-hours. Rounding is half away
from zero so C++ and Python match.
"""

from __future__ import annotations

import math


def round_half_away(value: float) -> int:
    if value >= 0.0:
        return int(math.floor(value + 0.5))
    return int(math.ceil(value - 0.5))


def scale_milli(value: float) -> int:
    return round_half_away(value * 1000.0)


def wh_to_micro(value: float) -> int:
    return round_half_away(value * 1000000.0)


def floor_div(numer: int, denom: int) -> int:
    if denom < 0:
        numer = -numer
        denom = -denom
    return int(numer // denom)


def div_round(numer: int, denom: int) -> int:
    if denom < 0:
        numer = -numer
        denom = -denom
    if numer >= 0:
        return (numer + denom // 2) // denom
    return -((-numer + denom // 2) // denom)


def hypot_mm(dx: int, dy: int, dz: int = 0) -> int:
    return round_half_away(math.sqrt(float(dx * dx + dy * dy + dz * dz)))
