"""Escape planner for a person or animal near the parked drone.

Climb to the backup site only when the verdict is ESCAPE and the battery,
cover, and wind checks pass. PROP_LOCKOUT never requests motion.
"""

from __future__ import annotations

from drone_pc.common.params import Limits
from drone_pc.common.units import hypot_mm, scale_milli

ESCAPE_RANK = {
    "STAY": 0,
    "WATCH": 1,
    "DETER": 2,
    "ESCAPE": 3,
    "PROP_LOCKOUT": 4,
}

_TRACKED = ("person", "animal")


def verdict_for_distance_mm(distance_mm: int, limits: Limits) -> str:
    if distance_mm <= scale_milli(limits.escape_prop_lockout_range_m):
        return "PROP_LOCKOUT"
    if distance_mm <= scale_milli(limits.escape_escape_range_m):
        return "ESCAPE"
    if distance_mm <= scale_milli(limits.escape_deter_range_m):
        return "DETER"
    if distance_mm <= scale_milli(limits.escape_watch_range_m):
        return "WATCH"
    return "STAY"


def worst_escape(names: list[str]) -> str:
    if not names:
        return "STAY"
    return max(names, key=lambda name: ESCAPE_RANK[name])


def action_for(verdict: str, soc: float, cover_open: bool, wind_mps: float, limits: Limits, backup_m) -> str:
    if verdict == "PROP_LOCKOUT":
        return "DO_NOT_ARM"
    if verdict == "ESCAPE":
        soc_ok = scale_milli(soc) >= scale_milli(limits.escape_min_soc)
        wind_ok = scale_milli(wind_mps) <= scale_milli(limits.escape_max_wind_mps)
        if soc_ok and cover_open and wind_ok and backup_m is not None:
            return "CLIMB_TO_BACKUP"
        return "HOLD_AND_ALERT"
    if verdict == "DETER":
        return "ALERT_DETER"
    if verdict == "WATCH":
        return "ALERT_WATCH"
    return "NONE"


def climb_setpoint(drone_m, backup_m, climb_m: float) -> dict:
    return {
        "x_mm": scale_milli(float(backup_m[0])),
        "y_mm": scale_milli(float(backup_m[1])),
        "z_mm": scale_milli(float(drone_m[2])) + scale_milli(float(climb_m)),
    }


def requests_for_escape(action: str) -> list[str]:
    if action == "CLIMB_TO_BACKUP":
        return ["ARM", "TAKEOFF", "OFFBOARD_SETPOINT"]
    if action == "HOLD_AND_ALERT":
        return ["HOLD"]
    return []


def mission_event(action: str):
    if action == "CLIMB_TO_BACKUP":
        return "ESCAPE_TRIGGER"
    return None


def evaluate_escape(drone_m, detections, soc: float, cover_open: bool, wind_mps: float, limits: Limits, backup_m):
    verdicts = []
    for detection in detections:
        kind = str(detection["class"])
        if kind not in _TRACKED:
            continue
        dx = scale_milli(float(detection["x"]) - float(drone_m[0]))
        dy = scale_milli(float(detection["y"]) - float(drone_m[1]))
        dz = scale_milli(float(detection["z"]) - float(drone_m[2]))
        verdicts.append(verdict_for_distance_mm(hypot_mm(dx, dy, dz), limits))
    verdict = worst_escape(verdicts)
    action = action_for(verdict, soc, cover_open, wind_mps, limits, backup_m)
    setpoint = None
    if action == "CLIMB_TO_BACKUP":
        setpoint = climb_setpoint(drone_m, backup_m, limits.escape_climb_m)
    return {
        "verdict": verdict,
        "action": action,
        "setpoint": setpoint,
        "requests": requests_for_escape(action),
        "mission_event": mission_event(action),
    }
