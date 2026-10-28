"""Mission phases. Every handle() call logs a transition and a reason."""

from __future__ import annotations

TRANSITIONS = {
    ("SLEEP_CHARGING", "START_PREFLIGHT"): ("PREFLIGHT_GATE", "charge_ready_start_preflight"),
    ("SLEEP_CHARGING", "ESCAPE_TRIGGER"): ("ESCAPE", "parked_escape"),
    ("PREFLIGHT_GATE", "PREFLIGHT_GO"): ("TAKEOFF", "preflight_go"),
    ("PREFLIGHT_GATE", "PREFLIGHT_HOLD"): ("PREFLIGHT_GATE", "preflight_hold_ground"),
    ("PREFLIGHT_GATE", "PREFLIGHT_NO_GO"): ("SLEEP_CHARGING", "preflight_no_go"),
    ("PREFLIGHT_GATE", "PREFLIGHT_RTL"): ("SLEEP_CHARGING", "preflight_rtl_on_ground"),
    ("PREFLIGHT_GATE", "PREFLIGHT_ABORT"): ("SLEEP_CHARGING", "preflight_abort"),
    ("PREFLIGHT_GATE", "PREFLIGHT_LAND_NOW"): ("SLEEP_CHARGING", "preflight_land_now_on_ground"),
    ("TAKEOFF", "TAKEOFF_COMPLETE"): ("CRUISE", "takeoff_altitude_reached"),
    ("TAKEOFF", "LINK_RTL"): ("RETURN", "link_loss_rtl"),
    ("TAKEOFF", "LINK_ABORT"): ("LAND", "link_loss_abort"),
    ("TAKEOFF", "ENERGY_RETURN"): ("RETURN", "energy_return"),
    ("TAKEOFF", "LAND_NOW"): ("LAND", "land_now"),
    ("CRUISE", "REACHED_SURVEY"): ("SURVEY", "entered_survey_area"),
    ("CRUISE", "ENERGY_RETURN"): ("RETURN", "energy_return"),
    ("CRUISE", "LINK_RTL"): ("RETURN", "link_loss_rtl"),
    ("CRUISE", "LINK_ABORT"): ("LAND", "link_loss_abort"),
    ("CRUISE", "LAND_NOW"): ("LAND", "land_now"),
    ("SURVEY", "SURVEY_COMPLETE"): ("RETURN", "survey_complete"),
    ("SURVEY", "ENERGY_RETURN"): ("RETURN", "energy_return"),
    ("SURVEY", "LINK_RTL"): ("RETURN", "link_loss_rtl"),
    ("SURVEY", "LINK_ABORT"): ("LAND", "link_loss_abort"),
    ("SURVEY", "LAND_NOW"): ("LAND", "land_now"),
    ("RETURN", "REACHED_LAND_ZONE"): ("LAND", "reached_landing_zone"),
    ("RETURN", "LINK_ABORT"): ("LAND", "link_loss_abort"),
    ("RETURN", "LAND_NOW"): ("LAND", "land_now"),
    ("LAND", "TOUCHDOWN"): ("SLEEP_CHARGING", "touchdown_sleep_charge"),
    ("LAND", "ESCAPE_TRIGGER"): ("ESCAPE", "landed_escape"),
    ("ESCAPE", "ESCAPE_COMPLETE"): ("LAND", "escape_reached_backup"),
    ("ESCAPE", "LINK_ABORT"): ("LAND", "link_loss_abort"),
    ("ESCAPE", "LAND_NOW"): ("LAND", "land_now"),
}


def transition_line(t_ms: int, previous: str, nxt: str, reason: str) -> str:
    return f"t={int(t_ms)} kind=transition from={previous} to={nxt} reason={reason}"


class StateMachine:
    def __init__(self, state: str = "SLEEP_CHARGING"):
        self.state = state
        self.logs: list[str] = []

    def handle(self, event: str, t_ms: int):
        previous = self.state
        mapped = TRANSITIONS.get((previous, event))
        if mapped is None:
            nxt, reason = previous, "ignored_event"
        else:
            nxt, reason = mapped
        line = transition_line(t_ms, previous, nxt, reason)
        self.logs.append(line)
        self.state = nxt
        return {"state": nxt, "reason": reason, "line": line}
