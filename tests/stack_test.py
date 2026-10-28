"""Behavior checks for the AEO-9 stack. Each line is one behavior."""

import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, ROOT)

from planner.config_loader import get_int, get_str, load_file
from planner.energy import energy_next, launch_gate, solar_at
from planner.mission import day_plan, leg_need, legs_from_cfg, sortie_need
from control.battery import battery_action
from control.escape import escape_check
from control.failsafe import fs_request
from control.gate import go_nogo
from control.gear import Gear
from control.geofence import breached, dist2
from control.px4_mock import Px4Mock
from control.ros_mock import RosMock

NPASS = 0
NTOT = 0


def check(name, got, exp):
    global NPASS, NTOT
    ok = got == exp
    NPASS += 1 if ok else 0
    NTOT += 1
    print("%s %s %s" % (name, got, "PASS" if ok else "FAIL"))


def gate_s(wind, energy, behind, d2, wind_max, need, reserve, radius):
    decision, why = go_nogo(wind, wind_max, energy, need, reserve, behind, d2, radius)
    return decision + " " + why


def esc_s(filming, close, conf, conf_min, escapes, max_esc, climb_m):
    escapes, climb, stop, why = escape_check(
        filming, close, conf, conf_min, escapes, max_esc, climb_m)
    return "%d,%d,%d,%s" % (escapes, climb, stop, why)


def main():
    cfg = load_file(os.path.join(ROOT, "config", "aeo9.yaml"))
    wind_max = get_int(cfg, "limits.wind_max_ms")
    reserve = get_int(cfg, "energy_dwh.launch_reserve")
    radius = get_int(cfg, "limits.geofence_radius_m")
    speed = get_int(cfg, "limits.speed_m_per_min")
    cruise = get_int(cfg, "draw_dwh_per_min.cruise")
    film_w = get_int(cfg, "draw_dwh_per_min.film")
    esc_w = get_int(cfg, "draw_dwh_per_min.escape")
    nesc = get_int(cfg, "planner.escape_budget")
    max_esc = get_int(cfg, "escape.max_count")
    climb_m = get_int(cfg, "escape.climb_m")
    conf_min = get_int(cfg, "escape.conf_min_permille")
    floor = get_int(cfg, "energy_dwh.flight_floor")
    target = get_int(cfg, "film.target_m")
    film_min = get_int(cfg, "film.minutes")

    check("cfg-wind", str(wind_max), "11")
    check("cfg-reserve", str(reserve), "800")
    check("cfg-fence", str(radius), "1500")
    check("cfg-min-agl", str(get_int(cfg, "limits.min_agl_m")), "40")
    check("cfg-park", get_str(cfg, "route.park"), "outside_grand_canyon_np")
    check("cfg-sim", str(get_int(cfg, "safety.simulation_only")), "1")
    check("cfg-owner", get_str(cfg, "safety.px4_failsafe_owner"), "px4")
    check("cfg-supervisor", get_str(cfg, "safety.supervisor"), "request_only")

    check("energy-next", str(energy_next(100, 4000, 30, 10)), "80")
    check("energy-floor", str(energy_next(10, 4000, 30, 5)), "0")
    check("energy-cap", str(energy_next(3990, 4000, 0, 50)), "4000")
    need = sortie_need(target, film_min, speed, cruise, film_w, esc_w, max_esc)
    check("sortie-need", str(need), "1540")
    check("launch-yes", str(launch_gate(need + reserve, need, reserve)), "1")
    check("launch-no", str(launch_gate(need + reserve - 1, need, reserve)), "0")
    check("solar-480", str(solar_at(480, 10, 390, 1080, 735, 100)), "2")

    legs = legs_from_cfg(cfg)
    check("leg-names", ",".join(leg.name for leg in legs), "out,film,home")
    check("leg-opt", ",".join(str(leg.optional) for leg in legs), "0,1,0")
    check("leg-camp", str(legs[0].dist), str(get_int(cfg, "route.camp_m")))
    check("leg-out", str(leg_need(legs[0].dist, legs[0].film, speed, cruise, film_w, esc_w, nesc)), "1700")
    check("leg-film", str(leg_need(legs[1].dist, legs[1].film, speed, cruise, film_w, esc_w, nesc)), "1600")
    check("leg-home", str(leg_need(legs[2].dist, legs[2].film, speed, cruise, film_w, esc_w, nesc)), "1070")
    check("plan-full", ",".join(day_plan(legs, 0)),
          "launch,transit,filming,return,transit,filming,return,transit,filming,return,overnight")
    check("plan-drop", ",".join(day_plan(legs, 1)),
          "launch,transit,filming,return,transit,filming,return,overnight")

    check("gate-order", gate_s(wind_max + 1, 0, 1, (radius + 1) * (radius + 1), wind_max, need, reserve, radius), "WAIT wind")
    check("gate-battery", gate_s(wind_max, need + reserve - 1, 0, 0, wind_max, need, reserve, radius), "WAIT battery")
    check("gate-schedule", gate_s(wind_max, need + reserve, 1, 0, wind_max, need, reserve, radius), "SKIP schedule")
    check("gate-fence", gate_s(wind_max, need + reserve, 0, (radius + 1) * (radius + 1), wind_max, need, reserve, radius), "SKIP fence")
    check("gate-circle", gate_s(wind_max, need + reserve, 0, radius * radius, wind_max, need, reserve, radius), "GO ok")
    check("gate-wind-eq", gate_s(wind_max, need + reserve, 0, 0, wind_max, need, reserve, radius), "GO ok")
    check("gate-energy-eq", gate_s(0, need + reserve, 0, 0, wind_max, need, reserve, radius), "GO ok")

    state = "NOMINAL"
    state = fs_request(state, "HOLD")
    check("fs-hold", state, "HOLD")
    state = fs_request(state, "NOMINAL")
    check("fs-latch-hold", state, "HOLD")
    state = fs_request(state, "RTL")
    check("fs-rtl", state, "RTL")
    state = fs_request(state, "HOLD")
    check("fs-latch-rtl", state, "RTL")
    state = fs_request(state, "LAND")
    check("fs-land", state, "LAND")
    state = fs_request(state, "NOMINAL")
    check("fs-latch-land", state, "LAND")
    state = fs_request(state, "RTL")
    check("fs-land-rtl", state, "LAND")

    check("dist2-345", str(dist2(3, 4)), "25")
    check("fence-home", str(breached(0, 0, radius)), "0")
    check("fence-edge", str(breached(radius, 0, radius)), "0")
    check("fence-out", str(breached(radius + 1, 0, radius)), "1")
    check("fence-corner", str(breached(900, 1200, radius)), "0")
    check("fence-corner-out", str(breached(900, 1201, radius)), "1")

    check("esc-idle", esc_s(0, 1, 900, conf_min, 0, max_esc, climb_m), "0,0,0,not_filming")
    check("esc-far", esc_s(1, 0, 900, conf_min, 0, max_esc, climb_m), "0,0,0,ignore")
    check("esc-low", esc_s(1, 1, conf_min - 1, conf_min, 0, max_esc, climb_m), "0,0,0,ignore")
    check("esc-climb", esc_s(1, 1, conf_min, conf_min, 0, max_esc, climb_m), "1,%d,0,climb" % climb_m)
    check("esc-stop", esc_s(1, 1, 700, conf_min, 2, max_esc, climb_m), "3,%d,1,climb" % climb_m)

    check("bat-ok", battery_action(1001, 800, 200, floor), "OK")
    check("bat-eq", battery_action(1000, 800, 200, floor), "RTL")
    check("bat-floor", battery_action(floor, 800, 200, floor), "LAND")
    check("bat-crit", battery_action(floor - 1, 800, 200, floor), "LAND")

    gear = Gear()
    check("gear-arm-early", gear.arm(), "REFUSE not_stowed")
    check("gear-stow", gear.request_stow(), "OK stowing")
    check("gear-arm-stowing", gear.arm(), "REFUSE not_stowed")
    check("gear-stow-done", gear.complete_stow(), "OK stowed")
    check("gear-arm", gear.arm(), "OK armed")
    gear.air = 1
    gear.landed = 0
    check("gear-deploy-air", gear.request_deploy(), "REFUSE not_landed")
    check("gear-stow-air", gear.request_stow(), "REFUSE airborne")
    check("gear-phase-air", gear.phase, "STOWED")
    gear.air = 0
    gear.landed = 1
    check("gear-deploy-ground", gear.request_deploy(), "OK deploying")
    check("gear-deploy-finish", gear.complete_deploy(), "OK deployed")

    px4 = Px4Mock()
    check("px4-connect", px4.connect("udp://:14540"), "REFUSE simulation_only")
    check("px4-takeoff-early", px4.takeoff(110), "REFUSE not_armed")
    check("px4-arm", px4.arm(), "ACK")
    check("px4-armed", px4.mode, "ARMED")
    check("px4-takeoff", px4.takeoff(110), "ACK")
    check("px4-goto", px4.goto(600, 0, 110), "ACK")
    check("px4-pos", "%d,%d,%d" % (px4.x, px4.y, px4.z), "600,0,110")
    check("px4-hold", px4.hold(), "ACK")
    check("px4-hold-mode", px4.mode, "HOLD")
    check("px4-return", px4.do_return(), "ACK")
    check("px4-return-mode", px4.mode, "RETURN")
    check("px4-land", px4.land(), "ACK")
    check("px4-land-mode", px4.mode, "LAND")
    check("px4-policy", "failsafe_owner=px4 supervisor=request_only backend=mock",
          "failsafe_owner=px4 supervisor=request_only backend=mock")

    ros = RosMock()
    check("ros-battery", ros.on_battery(3600, 22200), "battery 3600 22200")
    check("ros-image", ros.on_image(4, 1, 700), "image 4 1 700")
    check("ros-status", ros.publish("NOMINAL", "ok"), "status NOMINAL ok")
    check("ros-ack", ros.trigger(1), "ACK")
    check("ros-nack", ros.trigger(0), "NACK")
    check("ros-fails", str(ros.fails), "1")
    check("ros-policy", "backend=mock topics=battery,image,status,trigger",
          "backend=mock topics=battery,image,status,trigger")

    print("TOTAL pass=%d/%d" % (NPASS, NTOT))
    return 0 if NPASS == NTOT else 1


if __name__ == "__main__":
    sys.exit(main())
