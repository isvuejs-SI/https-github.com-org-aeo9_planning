from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum
from typing import Dict, List, Set


class MissionPhase(str, Enum):
    SLEEP_CHARGING = "SLEEP_CHARGING"
    PREFLIGHT_GATE = "PREFLIGHT_GATE"
    TAKEOFF = "TAKEOFF"
    CRUISE = "CRUISE"
    SURVEY = "SURVEY"
    RETURN = "RETURN"
    LAND = "LAND"
    ESCAPE = "ESCAPE"


ALLOWED: Dict[MissionPhase, Set[MissionPhase]] = {
    MissionPhase.SLEEP_CHARGING: {MissionPhase.PREFLIGHT_GATE, MissionPhase.ESCAPE},
    MissionPhase.PREFLIGHT_GATE: {
        MissionPhase.TAKEOFF,
        MissionPhase.SLEEP_CHARGING,
        MissionPhase.ESCAPE,
    },
    MissionPhase.TAKEOFF: {MissionPhase.CRUISE, MissionPhase.LAND, MissionPhase.ESCAPE},
    MissionPhase.CRUISE: {
        MissionPhase.SURVEY,
        MissionPhase.RETURN,
        MissionPhase.ESCAPE,
        MissionPhase.LAND,
    },
    MissionPhase.SURVEY: {MissionPhase.CRUISE, MissionPhase.RETURN, MissionPhase.ESCAPE},
    MissionPhase.RETURN: {MissionPhase.LAND, MissionPhase.ESCAPE},
    MissionPhase.LAND: {MissionPhase.SLEEP_CHARGING, MissionPhase.ESCAPE},
    MissionPhase.ESCAPE: {
        MissionPhase.RETURN,
        MissionPhase.LAND,
        MissionPhase.SLEEP_CHARGING,
    },
}


@dataclass
class TransitionRecord:
    from_phase: MissionPhase
    to_phase: MissionPhase
    reason: str


@dataclass
class StateMachine:
    current: MissionPhase = MissionPhase.SLEEP_CHARGING
    history: List[TransitionRecord] = field(default_factory=list)

    def can_transition(self, to: MissionPhase) -> bool:
        if to == self.current:
            return False
        return to in ALLOWED.get(self.current, set())

    def transition(self, to: MissionPhase, reason: str) -> bool:
        if not self.can_transition(to):
            return False
        self.history.append(TransitionRecord(self.current, to, reason))
        self.current = to
        return True
