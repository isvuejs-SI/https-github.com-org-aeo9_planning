from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum
from typing import List

from drone_pc.params import Params
from drone_pc.planning.occupancy_map import Point3


class Px4Request(str, Enum):
    NONE = "NONE"
    ARM = "ARM"
    TAKEOFF = "TAKEOFF"
    HOLD = "HOLD"
    RTL = "RTL"
    LAND = "LAND"
    OFFBOARD_SETPOINT = "OFFBOARD_SETPOINT"


@dataclass
class OffboardSetpoint:
    position: Point3
    yaw_rad: float = 0.0


@dataclass
class Px4InterfaceStatus:
    heartbeat_ok: bool = True
    missed_heartbeats: int = 0
    last_request: Px4Request = Px4Request.NONE
    fallback: str = ""


class Px4Interface:
    """Jetson sends requests only. PX4 owns final failsafes."""

    def __init__(self, params: Params):
        self.params = params
        self.status = Px4InterfaceStatus()
        self._time_since_hb = 0.0
        self.request_log: List[str] = []

    def offboard_period_s(self) -> float:
        return 1.0 / self.params.control_offboard_rate_hz

    def send_heartbeat(self) -> None:
        self._time_since_hb = 0.0
        self.status.missed_heartbeats = 0
        self.status.heartbeat_ok = True
        self.status.fallback = ""

    def tick(self, dt_s: float) -> None:
        self._time_since_hb += dt_s
        if self._time_since_hb > self.params.control_heartbeat_period_s:
            misses = int(self._time_since_hb / self.params.control_heartbeat_period_s)
            self.status.missed_heartbeats = misses
            if misses >= self.params.control_heartbeat_miss_limit:
                self.status.heartbeat_ok = False
                self.status.fallback = "hold_or_rtl"

    def request(self, req: Px4Request) -> bool:
        if req in (
            Px4Request.ARM,
            Px4Request.TAKEOFF,
            Px4Request.HOLD,
            Px4Request.RTL,
            Px4Request.LAND,
        ):
            self.status.last_request = req
            self.request_log.append(req.value)
            return True
        return False

    def send_offboard_setpoint(self, sp: OffboardSetpoint) -> bool:
        _ = sp
        self.status.last_request = Px4Request.OFFBOARD_SETPOINT
        self.request_log.append(Px4Request.OFFBOARD_SETPOINT.value)
        self.send_heartbeat()
        return True
