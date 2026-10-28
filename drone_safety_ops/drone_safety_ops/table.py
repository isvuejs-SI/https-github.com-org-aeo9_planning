"""Shared verdict table. The C++ binary prints the same text."""

import json

from drone_safety_ops.aggregate import Cache, evaluate
from drone_safety_ops.alert import Alerter
from drone_safety_ops.day3 import render as render_day3
from drone_safety_ops.guard import Detection, decide as guard_decide
from drone_safety_ops.landing import next_leg
from drone_safety_ops.mission_gate import MissionState, gate, return_wh
from drone_safety_ops.params import load_params, package_root
from drone_safety_ops.px4_request import (
    combine_requests,
    command_signature,
    fill_command,
    map_request,
    mavsdk_action,
)
from drone_safety_ops.route import plan
from drone_safety_ops.supervisor import SupervisorState, decide, deploy
from drone_safety_ops.verdict_log import VerdictLog, format_record
from drone_safety_ops.watchdog import systemd_watchdog_sec, watchdog_roundtrip

import os


def fmt_num(value):
    if abs(value - round(value)) < 1e-9:
        return f"{value:.0f}"
    return f"{value:.1f}"


def parked_ready(params):
    cache = Cache()
    cache.soc_pct = 100
    cache.cell_temp_c = 25
    cache.pack_wh = params.mission_gate.default_pack_wh
    cache.wind_ms = 3
    cache.wind_ok = True
    cache.gps_ok = True
    cache.inside_fence = True
    cache.legal_area_ok = True
    cache.geofence_ok = True
    cache.remote_id_ok = True
    cache.observer_ok = True
    cache.detector_ok = True
    cache.props_ok = True
    cache.wh_per_km = params.mission_gate.wh_per_km
    return cache


def survey_green(params):
    cache = parked_ready(params)
    cache.airborne = True
    cache.armed = True
    cache.soc_pct = 80
    cache.cell_temp_c = 30
    cache.wind_ms = 4
    cache.alt_m = 40
    cache.dist_home_km = 4
    cache.phase = "TRANSIT"
    cache.link_gap_s = 2
    return cache


def snapshot_line(name, cache, params):
    snap = evaluate(cache, params)
    return (
        f"sensor {name} worst={snap['worst_module']}:{snap['worst']} px4={snap['px4'].wire} "
        f"latch={1 if snap['px4'].latch_no_arm else 0} supervisor={snap['supervisor']} deploy={snap['deploy']} "
        f"gate={snap['gate']}:{snap['gate_why']} guard={snap['guard']} landing={snap['landing']} "
        f"landing_votes={1 if snap['landing_votes'] else 0} route={snap['route']}"
    )


def render(params_path=None, service_path=None):
    root = package_root()
    params_path = params_path or os.path.join(root, "config", "params.yaml")
    service_path = service_path or os.path.join(root, "systemd", "drone-safety-ops.service")
    params = load_params(params_path)
    lines = ["== params =="]
    for key, labeled in (
        ("landing.leg_km", params.landing.leg_km),
        ("landing.batt_endurance_min", params.landing.batt_endurance_min),
        ("landing.cruise_kmh", params.landing.cruise_kmh),
        ("route.leg_km", params.route.leg_km),
        ("route.batt_endurance_min", params.route.batt_endurance_min),
        ("route.cruise_kmh", params.route.cruise_kmh),
    ):
        lines.append(f"{key} {fmt_num(labeled.value)} {labeled.origin}")
    lines.append(f"alert.holdback_s {fmt_num(params.alert.duplicate_holdback_s)}")
    lines.append(f"mission.max_days {params.mission_gate.max_days}")
    lines.append(f"mission.link_rtl_s {fmt_num(params.mission_gate.link_rtl_s)}")
    lines.append(f"mission.link_abort_s {fmt_num(params.mission_gate.link_abort_s)}")
    watched = systemd_watchdog_sec(service_path)
    match = "match" if watched == fmt_num(params.watchdog.systemd_watchdog_sec) else "mismatch"
    lines.append(f"systemd_watchdog_sec {watched} {match}")

    lines.append("== supervisor ==")
    states = [
        SupervisorState("preflight, Remote ID fail", False, False, False, True, True, True, False, 95, 25, 3, False, 0, False),
        SupervisorState("survey all green", True, True, True, True, True, True, False, 80, 30, 4, False, 40, False),
        SupervisorState("detector timed out", True, True, True, True, True, False, False, 78, 30, 4, False, 40, False),
        SupervisorState("animal threat", True, True, True, True, True, True, True, 75, 30, 4, False, 15, False),
        SupervisorState("animal + geofence breach", True, True, True, True, False, True, True, 70, 30, 4, False, 15, False),
        SupervisorState("low battery 22%", True, True, True, True, True, True, False, 22, 35, 4, False, 30, False),
        SupervisorState("cell overtemp 65C", True, True, True, True, True, True, False, 60, 65, 4, False, 30, False),
        SupervisorState("GPS lost", True, True, True, False, True, True, False, 60, 30, 4, False, 30, False),
        SupervisorState("descending at 3 m", True, True, True, True, True, True, False, 40, 30, 4, False, 3, False),
        SupervisorState("landed but still armed", False, True, True, True, True, True, False, 40, 30, 4, False, 0, False),
        SupervisorState("landed, disarmed, windy", False, False, True, True, True, True, False, 40, 30, 14, False, 0, False),
        SupervisorState("landed, disarmed, calm", False, False, True, True, True, True, False, 40, 30, 3, False, 0, False),
        SupervisorState("cover open, calm", False, False, True, True, True, True, False, 40, 30, 3, False, 0, True),
    ]
    for state in states:
        lines.append(f"{state.scene} -> {decide(state, params.supervisor)} | deploy {deploy(state, params.supervisor)}")

    lines.append("== mission_gate ==")
    missions = [
        MissionState("D1 launch, park area", "LAUNCH", 1, False, True, True, 100, 400, 0, 25, 0, 0, 3, 25, False),
        MissionState("D1 launch, all green", "LAUNCH", 1, True, True, True, 100, 400, 0, 25, 0, 0, 3, 25, False),
        MissionState("D1 transit, healthy", "TRANSIT", 1, True, True, True, 80, 400, 4, 25, 0, 2, 5, 30, False),
        MissionState("D2 filming, reserve hit", "FILMING", 2, True, True, True, 45, 400, 6, 25, 0, 1, 4, 32, False),
        MissionState("D2 filming, link 45 s", "FILMING", 2, True, True, True, 70, 400, 3, 25, 0, 45, 4, 30, False),
        MissionState("D2 filming, observer lost", "FILMING", 2, True, False, True, 70, 400, 3, 25, 0, 1, 4, 30, False),
        MissionState("D2 night, good sun", "OVERNIGHT", 2, True, True, True, 30, 400, 8, 25, 300, 900, 4, 20, False),
        MissionState("D3 night, cloudy", "OVERNIGHT", 3, True, True, True, 30, 400, 8, 25, 40, 900, 4, 20, False),
        MissionState("D3 night, intrusion", "OVERNIGHT", 3, True, True, True, 60, 400, 8, 25, 300, 0, 3, 20, True),
        MissionState("D4 transit, link lost", "TRANSIT", 4, True, True, True, 90, 400, 5, 25, 0, 700, 4, 30, False),
        MissionState("D5 return, hot cell", "RETURN", 5, True, True, True, 60, 400, 2, 25, 0, 1, 4, 64, False),
        MissionState("D6 still out", "RETURN", 6, True, True, True, 90, 400, 2, 25, 0, 1, 4, 30, False),
    ]
    for state in missions:
        verdict, why = gate(state, params.mission_gate)
        lines.append(f"{state.name:<26} need {return_wh(state, params.mission_gate):6.1f} Wh -> {verdict:<13} {why}")

    lines.append("== day3 ==")
    text = render_day3(params)
    if text.endswith("\n"):
        text = text[:-1]
    lines.extend(text.split("\n"))

    lines.append("== guard ==")
    guards = [
        Detection("none", 0, 0, 80, True, True),
        Detection("animal", 25, 1, 80, True, True),
        Detection("animal", 10, 0, 80, True, True),
        Detection("animal", 6, 1.0, 80, True, True),
        Detection("animal", 6, 1.0, 20, True, True),
        Detection("animal", 6, 1.0, 80, False, True),
        Detection("human", 12, 0.5, 80, True, True),
        Detection("human", 3, 0, 80, True, True),
    ]
    for item in guards:
        lines.append(
            f"{item.kind} {fmt_num(item.dist)} {fmt_num(item.closing)} {fmt_num(item.soc)} "
            f"{1 if item.cover_open else 0} {1 if item.wind_ok else 0} -> {guard_decide(item, params.guard)}"
        )

    lines.append("== alert ==")
    clock = {"t": 1000.0}
    alerter = Alerter(params.alert.duplicate_holdback_s, clock=lambda: clock["t"])
    for verdict in ("STAY", "WATCH", "WATCH", "DETER", "ESCAPE"):
        sent, line = alerter.alert(verdict, f"{verdict}: animal 6 m, closing", dry=True)
        if sent:
            lines.append(line)

    lines.append("== alert_holdback ==")
    clock2 = {"t": 2000.0}
    hold = Alerter(params.alert.duplicate_holdback_s, clock=lambda: clock2["t"])

    def ping(verdict):
        sent, _line = hold.alert(verdict, "again", dry=True)
        lines.append(f"t{fmt_num(clock2['t'])} {verdict} {'true' if sent else 'false'}")

    ping("WATCH")
    ping("WATCH")
    clock2["t"] = 2059
    ping("WATCH")
    clock2["t"] = 2060
    ping("WATCH")
    ping("STAY")
    sent, line = hold.alert("PROP_LOCKOUT", "person 3 m", dry=True)
    if sent:
        lines.append(line)

    lines.append("== landing ==")
    for soc, cell, leg, props, geo, rid in (
        (1.00, 30, 44.5, True, True, True),
        (1.00, 30, 5, True, True, True),
        (0.97, 50, 5, True, True, True),
        (0.60, 30, 5, True, True, True),
        (1.00, 30, 5, False, True, True),
        (1.00, 30, 44.5, True, True, False),
    ):
        verdict, need, have, req = next_leg(soc, cell, props, geo, rid, params.landing, leg)
        lines.append(f"{verdict} {need:.1f} {have:.1f} {req:.0f}")

    lines.append("== route ==")
    straight = params.route.straight_line_mi * params.route.mi_to_km
    for real, tribal, waiver, park in (
        (False, False, False, True),
        (True, False, False, True),
        (True, True, False, True),
        (True, True, True, True),
        (False, False, False, False),
    ):
        verdict, pack, legs, req = plan(straight, params.route, real, tribal, waiver, park)
        lines.append(f"{verdict} {pack:.1f} {legs} {req:.0f}")

    lines.append("== px4 ==")
    for verdict, air in (
        ("CONTINUE", True), ("HOLD", True), ("CLIMB_ESCAPE", True), ("RTL", True), ("LAND", True),
        ("NO_TAKEOFF", False), ("PROP_LOCKOUT", False), ("PROP_LOCKOUT", True), ("ESCAPE", False),
        ("NO_GO", False), ("NO_GO", True), ("ABORT_MISSION", True), ("ABORT_MISSION", False),
        ("LAND_NOW", True), ("HOLD_GROUND", False), ("DETER", False), ("GO_SIM", False),
        ("STAY", False), ("WATCH", False),
    ):
        cmd = map_request(verdict, air)
        lines.append(f"map {verdict} {'air' if air else 'ground'} -> {cmd.wire} latch={1 if cmd.latch_no_arm else 0}")
    for wire in ("hold", "rtl", "land", "no_arm"):
        lines.append("cmd " + command_signature(fill_command(wire, wire == "no_arm")))
    for wire in ("hold", "rtl", "land", "no_arm"):
        lines.append(f"mavsdk {wire} {mavsdk_action(wire)}")
    combo = combine_requests([map_request("PROP_LOCKOUT", True), map_request("RTL", True)], True)
    lines.append(f"combine air PROP_LOCKOUT+RTL -> {combo.wire} latch={1 if combo.latch_no_arm else 0}")

    lines.append("== log ==")
    log = VerdictLog("/tmp/drone_safety_ops_table_py")
    ts = "2026-01-01T00:00:00Z"
    for record in (
        log.append(ts, "guard", "STAY", "none", ""),
        log.append(ts, "guard", "PROP_LOCKOUT", "no_arm", "person 3 m"),
        log.append(ts, "guard", "PROP_LOCKOUT", "no_arm", "person 3 m"),
    ):
        lines.append(format_record(record))

    lines.append("== watchdog ==")
    lines.append(watchdog_roundtrip("/tmp/dso-wd-py"))

    lines.append("== json ==")
    parsed = json.loads('{"soc_pct":22.5,"airborne":true,"kind":"human"}')
    lines.append(f"soc {fmt_num(parsed['soc_pct'])} airborne {1 if parsed['airborne'] else 0} kind {parsed['kind']}")

    lines.append("== sensors ==")
    lines.append(snapshot_line("unconfigured", Cache(), params))
    failed = parked_ready(params)
    failed.remote_id_ok = False
    lines.append(snapshot_line("parked_remote_id_fail", failed, params))
    lines.append(snapshot_line("survey_green", survey_green(params), params))
    low = survey_green(params)
    low.soc_pct = 22
    low.cell_temp_c = 35
    lines.append(snapshot_line("air_low_battery", low, params))
    lost = survey_green(params)
    lost.link_gap_s = 700
    lost.soc_pct = 90
    lost.dist_home_km = 5
    lines.append(snapshot_line("air_link_lost", lost, params))
    person = parked_ready(params)
    person.lidar_kind = "human"
    person.lidar_dist = 3
    person.lidar_closing = 0
    lines.append(snapshot_line("parked_person_3m", person, params))
    flying = survey_green(params)
    flying.lidar_kind = "human"
    flying.lidar_dist = 3
    lines.append(snapshot_line("air_person_3m", flying, params))
    night = parked_ready(params)
    night.phase = "OVERNIGHT"
    night.soc_pct = 35
    night.dist_home_km = 8
    night.solar_forecast_wh = 40
    night.link_gap_s = 900
    night.wind_ms = 14
    night.lidar_kind = "human"
    night.lidar_dist = 6
    night.intrusion = True
    lines.append(snapshot_line("overnight_intrusion", night, params))
    return "\n".join(lines) + "\n"


def main(argv=None):
    import sys
    argv = list(sys.argv[1:] if argv is None else argv)
    params = argv[0] if argv else None
    service = argv[1] if len(argv) > 1 else None
    sys.stdout.write(render(params, service))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
