"""Gear/solar deploy: stow before arming, deploy only after landing."""

from __future__ import annotations

from dataclasses import dataclass
from enum import Enum


class GearActionResult(str, Enum):
    OK = "OK"
    REFUSED = "REFUSED"


@dataclass
class GearController:
    stowed: bool = True
    armed: bool = False
    landed: bool = True

    def stow(self) -> tuple[GearActionResult, str]:
        self.stowed = True
        return GearActionResult.OK, "GEAR_STOWED"

    def arm(self) -> tuple[GearActionResult, str]:
        if not self.stowed:
            return GearActionResult.REFUSED, "ARM_REFUSED_GEAR_DEPLOYED"
        self.armed = True
        self.landed = False
        return GearActionResult.OK, "ARMED"

    def deploy(self) -> tuple[GearActionResult, str]:
        if not self.landed:
            return GearActionResult.REFUSED, "DEPLOY_REFUSED_NOT_LANDED"
        if self.armed:
            return GearActionResult.REFUSED, "DEPLOY_REFUSED_STILL_ARMED"
        self.stowed = False
        return GearActionResult.OK, "GEAR_DEPLOYED"

    def land(self) -> tuple[GearActionResult, str]:
        self.landed = True
        self.armed = False
        return GearActionResult.OK, "LANDED"

    def dump_line(self) -> str:
        return (
            f"gear stowed={1 if self.stowed else 0} "
            f"armed={1 if self.armed else 0} "
            f"landed={1 if self.landed else 0}"
        )
