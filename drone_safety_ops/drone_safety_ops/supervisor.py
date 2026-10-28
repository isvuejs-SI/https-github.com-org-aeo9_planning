from dataclasses import dataclass


CONTINUE, HOLD, CLIMB_ESCAPE, RTL, LAND, NO_TAKEOFF = range(6)
ACTION_NAME = ["CONTINUE", "HOLD", "CLIMB_ESCAPE", "RTL", "LAND", "NO_TAKEOFF"]
BLOCKED, GEAR_DOWN, OPEN_COVER, DEPLOY_PANELS = range(4)
DEPLOY_NAME = ["BLOCKED", "GEAR_DOWN", "OPEN_COVER", "DEPLOY_PANELS"]


@dataclass
class SupervisorState:
    scene: str = ""
    airborne: bool = False
    armed: bool = False
    remote_id_ok: bool = False
    gps_ok: bool = False
    inside_fence: bool = False
    detector_ok: bool = False
    animal: bool = False
    batt_pct: float = 0
    cell_temp_c: float = 0
    wind_ms: float = 0
    rain: bool = False
    alt_m: float = 0
    cover_open: bool = False


def decide(state, params):
    verdict = CONTINUE
    if not state.airborne:
        if (not state.remote_id_ok or not state.gps_ok or state.batt_pct < params.batt_no_takeoff_pct
                or state.cell_temp_c > params.cell_temp_limit_c):
            verdict = max(verdict, NO_TAKEOFF)
        return ACTION_NAME[verdict]
    if not state.detector_ok:
        verdict = max(verdict, HOLD)
    if state.animal:
        verdict = max(verdict, CLIMB_ESCAPE)
    if not state.inside_fence:
        verdict = max(verdict, RTL)
    if state.batt_pct < params.batt_rtl_pct:
        verdict = max(verdict, RTL)
    if not state.gps_ok:
        verdict = max(verdict, LAND)
    if state.batt_pct < params.batt_land_pct or state.cell_temp_c > params.cell_temp_limit_c:
        verdict = max(verdict, LAND)
    return ACTION_NAME[verdict]


def deploy(state, params):
    if state.airborne:
        return DEPLOY_NAME[GEAR_DOWN if state.alt_m < params.gear_down_alt_m else BLOCKED]
    if state.armed or state.wind_ms > params.deploy_wind_max_ms or state.rain:
        return DEPLOY_NAME[BLOCKED]
    return DEPLOY_NAME[DEPLOY_PANELS if state.cover_open else OPEN_COVER]
