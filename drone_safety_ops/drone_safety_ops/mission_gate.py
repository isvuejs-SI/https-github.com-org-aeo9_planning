from dataclasses import dataclass

GO, HOLD_GROUND, RTL, ABORT_MISSION, LAND_NOW, NO_GO = range(6)
GATE_NAME = ["GO", "HOLD_GROUND", "RTL", "ABORT_MISSION", "LAND_NOW", "NO_GO"]


@dataclass
class MissionState:
    name: str = ""
    phase: str = "LAUNCH"
    day: int = 1
    legal_area_ok: bool = False
    observer_ok: bool = False
    remote_id_ok: bool = False
    soc_pct: float = 0
    pack_wh: float = 0
    dist_home_km: float = 0
    wh_per_km: float = 0
    solar_forecast_wh: float = 0
    link_gap_s: float = 0
    wind_ms: float = 0
    cell_temp_c: float = 0
    intrusion: bool = False


def return_wh(state, params):
    return state.dist_home_km * state.wh_per_km * params.energy_margin + params.reserve_fraction * state.pack_wh


def gate(state, params):
    verdict = GO
    why = []

    def up(level, reason):
        nonlocal verdict
        if level > verdict:
            verdict = level
        if level > GO:
            why.append(reason)

    have = state.soc_pct / 100.0 * state.pack_wh
    night = state.phase == "OVERNIGHT"
    if not state.legal_area_ok:
        up(NO_GO, "outside legal area")
    if (state.phase == "LAUNCH" or night) and not state.remote_id_ok:
        up(NO_GO, "remote id")
    if not night and not state.observer_ok:
        up(RTL, "no visual observer")
    if state.day > params.max_days:
        up(ABORT_MISSION, "past day budget")
    if night:
        if state.intrusion:
            up(HOLD_GROUND, "intrusion: stay locked")
        if have + state.solar_forecast_wh < return_wh(state, params):
            up(ABORT_MISSION, "solar cannot cover return")
        if state.wind_ms > params.overnight_wind_hold_ms:
            up(HOLD_GROUND, "wind: panels stowed")
    else:
        if have < return_wh(state, params):
            up(RTL, "return reserve")
        if state.link_gap_s > params.link_rtl_s:
            up(RTL, "link gap")
        if state.link_gap_s > params.link_abort_s:
            up(ABORT_MISSION, "link lost")
        if state.wind_ms > params.flight_wind_rtl_ms:
            up(RTL, "wind")
    if state.cell_temp_c > params.cell_overtemp_c:
        up(ABORT_MISSION if night else LAND_NOW, "cell overtemp")
    return GATE_NAME[verdict], ("; ".join(why) if why else "ok")
