import math


def plan(dist_km, params, real_flight, tribal_ok, waiver, park_clear):
    """Real flight is NO_GO without tribal permission and a Part 107 waiver.

    GO_SIM is only for real_flight false. A leg longer than one pack is NO_GO
    when real_flight is true. This package does not authorize a real flight.
    """
    pack = (params.batt_endurance_min.value * (1.0 - params.reserve_fraction) / params.energy_margin
            * params.cruise_kmh.value / 60.0)
    legs = math.ceil(dist_km / pack)
    req = params.leg_km.value / (params.batt_endurance_min.value / 60.0)
    verdict = "GO_SIM"
    if not park_clear:
        verdict = "NO_GO"
    if real_flight and (not tribal_ok or not waiver):
        verdict = "NO_GO"
    if params.leg_km.value > pack and real_flight:
        verdict = "NO_GO"
    return verdict, pack, int(legs), req
