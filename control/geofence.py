"""Geofence: distance from home + breach handling."""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum


class GeofenceStatus(str, Enum):
    INSIDE = "INSIDE"
    BREACH = "BREACH"


@dataclass
class Geofence:
    home_north_cm: int
    home_east_cm: int
    radius_cm: int
    breached: bool = False

    def distance_cm(self, north_cm: int, east_cm: int) -> int:
        dn = abs(north_cm - self.home_north_cm)
        de = abs(east_cm - self.home_east_cm)
        if dn > de:
            return dn + de // 2
        return de + dn // 2

    def check(self, north_cm: int, east_cm: int) -> GeofenceStatus:
        dist = self.distance_cm(north_cm, east_cm)
        if dist > self.radius_cm:
            self.breached = True
            return GeofenceStatus.BREACH
        return GeofenceStatus.BREACH if self.breached else GeofenceStatus.INSIDE

    def dump_line(self, north_cm: int, east_cm: int) -> str:
        dist = self.distance_cm(north_cm, east_cm)
        status = self.check(north_cm, east_cm)
        return f"geofence status={status.value} dist_cm={dist} radius_cm={self.radius_cm}"
