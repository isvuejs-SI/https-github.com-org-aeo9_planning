from dataclasses import dataclass

LEVELS = ["STAY", "WATCH", "DETER", "ESCAPE", "PROP_LOCKOUT"]


@dataclass
class Detection:
    kind: str
    dist: float
    closing: float
    soc: float
    cover_open: bool
    wind_ok: bool


def decide(state, params):
    """PROP_LOCKOUT means never arm. A person inside human_lockout_m stays locked out."""
    if state.kind == "none":
        return "STAY"
    if state.kind == "human" and state.dist < params.human_lockout_m:
        return "PROP_LOCKOUT"
    if state.dist > params.watch_beyond_m:
        return "WATCH"
    if state.kind == "human":
        return "DETER"
    if state.dist < params.animal_escape_within_m and state.closing > params.animal_closing_mps:
        ok = state.soc >= params.escape_min_soc_pct and state.cover_open and state.wind_ok
        return "ESCAPE" if ok else "DETER"
    return "DETER"


def rank(verdict):
    return LEVELS.index(verdict) if verdict in LEVELS else 0


def worse(a, b):
    return a if rank(a) >= rank(b) else b
