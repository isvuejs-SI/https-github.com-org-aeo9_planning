"""Failsafe state machine with latch: NOMINAL → HOLD/RTL/LAND."""

from __future__ import annotations

from enum import Enum


class FailsafeState(str, Enum):
    NOMINAL = "NOMINAL"
    HOLD = "HOLD"
    RTL = "RTL"
    LAND = "LAND"


# Severity rank — transitions may only stay or escalate (latch).
_RANK = {
    FailsafeState.NOMINAL: 0,
    FailsafeState.HOLD: 1,
    FailsafeState.RTL: 2,
    FailsafeState.LAND: 3,
}


class FailsafeMachine:
    def __init__(self) -> None:
        self.state = FailsafeState.NOMINAL

    def request(self, target: FailsafeState) -> FailsafeState:
        """Apply request only if equal or higher severity (no flip-back)."""
        if _RANK[target] >= _RANK[self.state]:
            self.state = target
        return self.state

    def dump_line(self) -> str:
        return f"failsafe={self.state.value}"
