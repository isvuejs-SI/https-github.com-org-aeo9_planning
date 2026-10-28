from drone_safety_ops.guard import Detection, decide as guard_decide, worse as worse_guard
from drone_safety_ops.landing import next_leg
from drone_safety_ops.mission_gate import MissionState, gate
from drone_safety_ops.px4_request import combine_requests, map_request
from drone_safety_ops.route import plan
from drone_safety_ops.supervisor import SupervisorState, decide, deploy


SEVERITY = {
    "PROP_LOCKOUT": 100,
    "NO_GO": 90,
    "NO_TAKEOFF": 80,
    "LAND_NOW": 72,
    "LAND": 70,
    "ABORT_MISSION": 60,
    "RTL": 50,
    "CLIMB_ESCAPE": 40,
    "ESCAPE": 35,
    "HOLD": 30,
    "HOLD_GROUND": 28,
    "DETER": 20,
    "WATCH": 10,
}


class Cache:
    def __init__(self):
        self.soc_pct = 0.0
        self.cell_temp_c = 25.0
        self.pack_wh = 0.0
        self.airborne = False
        self.armed = False
        self.cover_open = False
        self.wind_ms = 0.0
        self.rain = False
        self.wind_ok = False
        self.gps_ok = False
        self.inside_fence = False
        self.alt_m = 0.0
        self.dist_home_km = 0.0
        self.legal_area_ok = False
        self.geofence_ok = False
        self.remote_id_ok = False
        self.observer_ok = False
        self.phase = "LAUNCH"
        self.day = 1
        self.solar_forecast_wh = 0.0
        self.link_gap_s = 0.0
        self.detector_ok = False
        self.lidar_kind = "none"
        self.lidar_dist = 0.0
        self.lidar_closing = 0.0
        self.thermal_kind = "none"
        self.thermal_dist = 0.0
        self.thermal_closing = 0.0
        self.props_ok = False
        self.intrusion = False
        self.wh_per_km = 0.0
        self.leg_km = 0.0


def _set(msg, key, dest, cast):
    if key in msg:
        return cast(msg[key])
    return dest


def apply_battery(cache, msg):
    cache.soc_pct = _set(msg, "soc_pct", cache.soc_pct, float)
    cache.cell_temp_c = _set(msg, "cell_temp_c", cache.cell_temp_c, float)
    cache.pack_wh = _set(msg, "pack_wh", cache.pack_wh, float)
    cache.airborne = _set(msg, "airborne", cache.airborne, bool)
    cache.armed = _set(msg, "armed", cache.armed, bool)
    cache.cover_open = _set(msg, "cover_open", cache.cover_open, bool)
    cache.wind_ms = _set(msg, "wind_ms", cache.wind_ms, float)
    cache.rain = _set(msg, "rain", cache.rain, bool)
    cache.wind_ok = _set(msg, "wind_ok", cache.wind_ok, bool)
    cache.props_ok = _set(msg, "props_ok", cache.props_ok, bool)


def apply_gps(cache, msg):
    cache.gps_ok = _set(msg, "gps_ok", cache.gps_ok, bool)
    cache.inside_fence = _set(msg, "inside_fence", cache.inside_fence, bool)
    cache.alt_m = _set(msg, "alt_m", cache.alt_m, float)
    cache.dist_home_km = _set(msg, "dist_home_km", cache.dist_home_km, float)
    cache.legal_area_ok = _set(msg, "legal_area_ok", cache.legal_area_ok, bool)
    cache.geofence_ok = _set(msg, "geofence_ok", cache.geofence_ok, bool)


def apply_link(cache, msg):
    cache.remote_id_ok = _set(msg, "remote_id_ok", cache.remote_id_ok, bool)
    cache.observer_ok = _set(msg, "observer_ok", cache.observer_ok, bool)
    cache.phase = _set(msg, "phase", cache.phase, str)
    cache.day = _set(msg, "day", cache.day, int)
    cache.solar_forecast_wh = _set(msg, "solar_forecast_wh", cache.solar_forecast_wh, float)
    cache.link_gap_s = _set(msg, "link_gap_s", cache.link_gap_s, float)
    cache.intrusion = _set(msg, "intrusion", cache.intrusion, bool)


def apply_lidar(cache, msg):
    cache.lidar_kind = _set(msg, "kind", cache.lidar_kind, str)
    cache.lidar_dist = _set(msg, "dist_m", cache.lidar_dist, float)
    cache.lidar_closing = _set(msg, "closing_mps", cache.lidar_closing, float)
    cache.intrusion = _set(msg, "intrusion", cache.intrusion, bool)


def apply_thermal(cache, msg):
    cache.thermal_kind = _set(msg, "kind", cache.thermal_kind, str)
    cache.thermal_dist = _set(msg, "dist_m", cache.thermal_dist, float)
    cache.thermal_closing = _set(msg, "closing_mps", cache.thermal_closing, float)
    cache.cell_temp_c = _set(msg, "cell_temp_c", cache.cell_temp_c, float)
    cache.intrusion = _set(msg, "intrusion", cache.intrusion, bool)
    if "kind" in msg or "dist_m" in msg:
        cache.detector_ok = True


def worst_vote(votes):
    best_rank = -1
    module = ""
    verdict = ""
    for name, value, counted in votes:
        if not counted:
            continue
        rank = SEVERITY.get(value, 0)
        if rank > best_rank:
            best_rank = rank
            module = name
            verdict = value
    return module, verdict


def evaluate(cache, params):
    sup = SupervisorState(
        airborne=cache.airborne,
        armed=cache.armed,
        remote_id_ok=cache.remote_id_ok,
        gps_ok=cache.gps_ok,
        inside_fence=cache.inside_fence,
        detector_ok=cache.detector_ok,
        animal=cache.lidar_kind == "animal" or cache.thermal_kind == "animal",
        batt_pct=cache.soc_pct,
        cell_temp_c=cache.cell_temp_c,
        wind_ms=cache.wind_ms,
        rain=cache.rain,
        alt_m=cache.alt_m,
        cover_open=cache.cover_open,
    )
    supervisor = decide(sup, params.supervisor)
    deploy_name = deploy(sup, params.supervisor)
    mission = MissionState(
        phase=cache.phase,
        day=cache.day,
        legal_area_ok=cache.legal_area_ok,
        observer_ok=cache.observer_ok,
        remote_id_ok=cache.remote_id_ok,
        soc_pct=cache.soc_pct,
        pack_wh=cache.pack_wh if cache.pack_wh > 0 else params.mission_gate.default_pack_wh,
        dist_home_km=cache.dist_home_km,
        wh_per_km=cache.wh_per_km if cache.wh_per_km > 0 else params.mission_gate.wh_per_km,
        solar_forecast_wh=cache.solar_forecast_wh,
        link_gap_s=cache.link_gap_s,
        wind_ms=cache.wind_ms,
        cell_temp_c=cache.cell_temp_c,
        intrusion=cache.intrusion or (
            cache.phase == "OVERNIGHT" and (cache.lidar_kind == "human" or cache.thermal_kind == "human")
        ),
    )
    gate_verdict, gate_why = gate(mission, params.mission_gate)
    lidar = Detection(cache.lidar_kind, cache.lidar_dist, cache.lidar_closing, cache.soc_pct, cache.cover_open,
                      cache.wind_ok)
    thermal = Detection(cache.thermal_kind, cache.thermal_dist, cache.thermal_closing, cache.soc_pct,
                        cache.cover_open, cache.wind_ok)
    guard = worse_guard(guard_decide(lidar, params.guard), guard_decide(thermal, params.guard))
    soc_fraction = cache.soc_pct / 100.0 if cache.soc_pct > 1.0 else cache.soc_pct
    leg = cache.leg_km if cache.leg_km > 0 else params.landing.leg_km.value
    landing, _, _, _ = next_leg(soc_fraction, cache.cell_temp_c, cache.props_ok, cache.geofence_ok,
                                cache.remote_id_ok, params.landing, leg)
    landing_votes = not cache.airborne
    route_verdict, _, _, _ = plan(
        params.route.straight_line_mi * params.route.mi_to_km,
        params.route,
        params.route.real_flight,
        params.route.tribal_permission,
        params.route.faa_waiver,
        cache.legal_area_ok,
    )
    votes = [
        ("supervisor", supervisor, True),
        ("mission_gate", gate_verdict, True),
        ("guard", guard, True),
        ("landing", landing, landing_votes),
        ("route", route_verdict, True),
    ]
    module, worst = worst_vote(votes)
    parts = [
        map_request(supervisor, cache.airborne),
        map_request(gate_verdict, cache.airborne),
        map_request(guard, cache.airborne),
    ]
    if landing_votes:
        parts.append(map_request(landing, cache.airborne))
    parts.append(map_request(route_verdict, cache.airborne))
    px4 = combine_requests(parts, cache.airborne)
    return {
        "supervisor": supervisor,
        "deploy": deploy_name,
        "gate": gate_verdict,
        "gate_why": gate_why,
        "guard": guard,
        "landing": landing,
        "landing_votes": landing_votes,
        "route": route_verdict,
        "worst_module": module,
        "worst": worst,
        "px4": px4,
    }
