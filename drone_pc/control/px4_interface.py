"""Requests sent toward PX4. This module never writes actuators or failsafes.

Allowed requests are arm, takeoff, hold, RTL, land, and a position
offboard setpoint. Arm is a vehicle-command request that PX4 may reject.
There is no actuator, motor, PWM, or failsafe-parameter write.
"""

from __future__ import annotations

from drone_pc.common.units import div_round, scale_milli

ALLOWED_REQUESTS = (
    "ARM",
    "TAKEOFF",
    "HOLD",
    "RTL",
    "LAND",
    "OFFBOARD_SETPOINT",
)

# PX4 owns these. drone_pc documents the expectation and does not set them.
PX4_OWNED_FAILSAFE_PARAMS = (
    "COM_OF_LOSS_T",
    "NAV_DLL_ACT",
    "COM_LOW_BAT_ACT",
    "COM_RCL_EXCEPT",
)

PHASE_REQUESTS = {
    "SLEEP_CHARGING": (),
    "PREFLIGHT_GATE": (),
    "TAKEOFF": ("ARM", "TAKEOFF"),
    "CRUISE": ("OFFBOARD_SETPOINT",),
    "SURVEY": ("OFFBOARD_SETPOINT",),
    "RETURN": ("RTL",),
    "LAND": ("LAND",),
    "ESCAPE": ("OFFBOARD_SETPOINT",),
}

SETPOINT_KEYS = ("x_mm", "y_mm", "z_mm")


def requests_for_phase(phase: str) -> list[str]:
    if phase not in PHASE_REQUESTS:
        raise KeyError(phase)
    return list(PHASE_REQUESTS[phase])


def requests_for_verdict(verdict: str, airborne: bool) -> list[str]:
    if verdict == "GO":
        return []
    if verdict == "HOLD_GROUND":
        return ["HOLD"] if airborne else []
    if verdict == "RTL":
        return ["RTL"]
    if verdict == "ABORT_MISSION":
        return ["LAND"]
    if verdict == "LAND_NOW":
        return ["LAND"]
    if verdict == "NO_GO":
        return []
    raise KeyError(verdict)


def make_request(kind: str, setpoint: dict | None = None) -> dict:
    if kind not in ALLOWED_REQUESTS:
        raise ValueError(
            "refusing request " + kind + "; PX4 owns actuators and failsafes"
        )
    body = {"kind": kind}
    if setpoint is not None:
        extra = [key for key in setpoint.keys() if key not in SETPOINT_KEYS]
        if extra:
            raise ValueError("setpoint accepts position only")
        body["setpoint"] = {key: int(setpoint[key]) for key in SETPOINT_KEYS if key in setpoint}
    return body


class OffboardLink:
    """Steady position-setpoint heartbeat. A miss is recorded, not overridden."""

    def __init__(self, rate_hz: float, timeout_s: float):
        self.period_ms = div_round(1000000, scale_milli(rate_hz))
        self.timeout_ms = scale_milli(timeout_s)
        self.last_ms = None

    def tick(self, now_ms: int) -> list[str]:
        if self.last_ms is None or int(now_ms) - self.last_ms >= self.period_ms:
            self.last_ms = int(now_ms)
            return ["OFFBOARD_SETPOINT"]
        return []

    def missed(self, now_ms: int) -> bool:
        if self.last_ms is None:
            return True
        return int(now_ms) - self.last_ms > self.timeout_ms


def missed_heartbeat_response() -> dict:
    """Jetson does not take over. PX4 is expected to Hold or RTL itself."""
    return {"jetson_action": "NONE", "px4_expected": "PX4_HOLD_OR_RTL"}
