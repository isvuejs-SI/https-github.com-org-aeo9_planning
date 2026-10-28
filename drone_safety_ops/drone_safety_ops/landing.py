ORDER = ["GO", "HOLD_GROUND", "NO_GO"]


def worst(a, b):
    return a if ORDER.index(a) >= ORDER.index(b) else b


def next_leg(soc, cell_c, props_ok, geofence_ok, rid_ok, params, leg_km):
    verdict = "GO"
    if not props_ok or not geofence_ok or not rid_ok:
        verdict = worst(verdict, "HOLD_GROUND")
    if cell_c >= params.cell_hold_c or soc < params.soc_hold_fraction:
        verdict = worst(verdict, "HOLD_GROUND")
    need = leg_km / params.cruise_kmh.value * 60.0 * params.energy_margin
    have = params.batt_endurance_min.value * (1.0 - params.reserve_fraction) * (soc if soc <= 1.0 else 1.0)
    if need > have:
        verdict = worst(verdict, "NO_GO")
    req = leg_km / (params.batt_endurance_min.value / 60.0)
    return verdict, need, have, req
