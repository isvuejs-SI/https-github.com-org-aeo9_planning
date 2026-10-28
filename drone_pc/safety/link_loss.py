"""Starlink / GCS link-loss policy.

Durations come from YAML. Over rtl_after_s the supervisor requests RTL.
Over abort_after_s it aborts the mission and requests land. PX4 still
applies its own datalink failsafe.
"""

from __future__ import annotations

from drone_pc.common.params import Limits
from drone_pc.common.units import scale_milli


def link_loss_action(lost_s: float, rtl_after_s: float, abort_after_s: float) -> str:
    lost_ms = scale_milli(lost_s)
    if lost_ms > scale_milli(abort_after_s):
        return "ABORT_MISSION"
    if lost_ms > scale_milli(rtl_after_s):
        return "RTL"
    return "CONTINUE"


def link_loss_action_from_limits(lost_s: float, limits: Limits) -> str:
    return link_loss_action(lost_s, limits.link_rtl_after_s, limits.link_abort_after_s)


def requests_for_link_action(action: str) -> list[str]:
    if action == "RTL":
        return ["RTL"]
    if action == "ABORT_MISSION":
        return ["LAND"]
    return []


def mission_event_for_link(action: str):
    if action == "RTL":
        return "LINK_RTL"
    if action == "ABORT_MISSION":
        return "LINK_ABORT"
    return None
