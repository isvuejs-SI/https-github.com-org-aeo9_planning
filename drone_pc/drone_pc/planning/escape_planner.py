from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum

from drone_pc.params import Params
from drone_pc.planning.occupancy_map import Point3


class EscapeVerdict(str, Enum):
    STAY = "STAY"
    WATCH = "WATCH"
    DETER = "DETER"
    ESCAPE = "ESCAPE"
    PROP_LOCKOUT = "PROP_LOCKOUT"


@dataclass
class EscapeInput:
    threat_verdict: EscapeVerdict = EscapeVerdict.STAY
    soc_percent: float = 100.0
    cover_open: bool = True
    wind_ok: bool = True
    current_position: Point3 = field(default_factory=Point3)
    backup_site: Point3 = field(default_factory=Point3)
    backup_valid: bool = False


@dataclass
class EscapeOutput:
    action: EscapeVerdict = EscapeVerdict.STAY
    execute_escape: bool = False
    target: Point3 = field(default_factory=Point3)
    reason: str = ""


class EscapePlanner:
    def __init__(self, params: Params):
        self.params = params

    def classify_threat(self, distance_m: float) -> EscapeVerdict:
        if distance_m <= self.params.escape_prop_lockout_radius_m:
            return EscapeVerdict.PROP_LOCKOUT
        if distance_m <= self.params.escape_escape_radius_m:
            return EscapeVerdict.ESCAPE
        if distance_m <= self.params.escape_deter_radius_m:
            return EscapeVerdict.DETER
        if distance_m <= self.params.escape_watch_radius_m:
            return EscapeVerdict.WATCH
        return EscapeVerdict.STAY

    def plan(self, inp: EscapeInput) -> EscapeOutput:
        out = EscapeOutput(action=inp.threat_verdict, target=inp.current_position)

        if inp.threat_verdict in (
            EscapeVerdict.STAY,
            EscapeVerdict.WATCH,
            EscapeVerdict.DETER,
        ):
            out.reason = "no_escape_required"
            return out

        if inp.threat_verdict == EscapeVerdict.PROP_LOCKOUT:
            out.reason = "prop_lockout_hold"
            return out

        if inp.soc_percent < self.params.energy_escape_min_soc_percent:
            out.reason = "escape_blocked_soc"
            return out
        if not inp.cover_open:
            out.reason = "escape_blocked_cover"
            return out
        if not inp.wind_ok:
            out.reason = "escape_blocked_wind"
            return out
        if not inp.backup_valid:
            out.reason = "escape_blocked_no_backup"
            return out

        out.execute_escape = True
        out.target = Point3(
            inp.backup_site.x, inp.backup_site.y, self.params.escape_climb_altitude_m
        )
        out.reason = "escape_to_backup"
        return out
