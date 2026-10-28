// C++ twin of tests/stack_test.py. Each line is one behavior.
#include "control/battery.hpp"
#include "control/escape.hpp"
#include "control/failsafe.hpp"
#include "control/gate.hpp"
#include "control/gear.hpp"
#include "control/geofence.hpp"
#include "control/px4_mock.hpp"
#include "control/ros_mock.hpp"
#include "planner/config_loader.hpp"
#include "planner/energy.hpp"
#include "planner/mission.hpp"
#include <cstdio>
#include <string>
#include <vector>

static int NPASS = 0;
static int NTOT = 0;

static void check(const std::string& name, const std::string& got, const std::string& exp) {
    int ok = got == exp;
    NPASS += ok ? 1 : 0;
    NTOT += 1;
    std::printf("%s %s %s\n", name.c_str(), got.c_str(), ok ? "PASS" : "FAIL");
}

static std::string gate_s(int wind, int energy, int behind, long long d2, int wind_max, int need,
                           int reserve, int radius) {
    Decision d = go_nogo(wind, wind_max, energy, need, reserve, behind, d2, radius);
    return d.action + " " + d.why;
}

static std::string esc_s(int filming, int close, int conf, int conf_min, int escapes, int max_esc,
                         int climb_m) {
    Esc e = escape_check(filming, close, conf, conf_min, escapes, max_esc, climb_m);
    char b[64];
    std::snprintf(b, sizeof b, "%d,%d,%d,%s", e.escapes, e.climb, e.stop, e.why.c_str());
    return b;
}

static std::string join_names(const std::vector<Leg>& legs) {
    std::string s;
    for (std::size_t i = 0; i < legs.size(); ++i) {
        if (i) s += ",";
        s += legs[i].name;
    }
    return s;
}

static std::string join_opt(const std::vector<Leg>& legs) {
    std::string s;
    for (std::size_t i = 0; i < legs.size(); ++i) {
        if (i) s += ",";
        s += std::to_string(legs[i].optional);
    }
    return s;
}

int main() {
    Tree cfg = load_file("../config/aeo9.yaml");
    int wind_max = get_int(cfg, "limits.wind_max_ms");
    int reserve = get_int(cfg, "energy_dwh.launch_reserve");
    int radius = get_int(cfg, "limits.geofence_radius_m");
    int speed = get_int(cfg, "limits.speed_m_per_min");
    int cruise = get_int(cfg, "draw_dwh_per_min.cruise");
    int film_w = get_int(cfg, "draw_dwh_per_min.film");
    int esc_w = get_int(cfg, "draw_dwh_per_min.escape");
    int nesc = get_int(cfg, "planner.escape_budget");
    int max_esc = get_int(cfg, "escape.max_count");
    int climb_m = get_int(cfg, "escape.climb_m");
    int conf_min = get_int(cfg, "escape.conf_min_permille");
    int floor = get_int(cfg, "energy_dwh.flight_floor");
    int target = get_int(cfg, "film.target_m");
    int film_min = get_int(cfg, "film.minutes");

    check("cfg-wind", std::to_string(wind_max), "11");
    check("cfg-reserve", std::to_string(reserve), "800");
    check("cfg-fence", std::to_string(radius), "1500");
    check("cfg-min-agl", std::to_string(get_int(cfg, "limits.min_agl_m")), "40");
    check("cfg-park", get_str(cfg, "route.park"), "outside_grand_canyon_np");
    check("cfg-sim", std::to_string(get_int(cfg, "safety.simulation_only")), "1");
    check("cfg-owner", get_str(cfg, "safety.px4_failsafe_owner"), "px4");
    check("cfg-supervisor", get_str(cfg, "safety.supervisor"), "request_only");

    check("energy-next", std::to_string(energy_next(100, 4000, 30, 10)), "80");
    check("energy-floor", std::to_string(energy_next(10, 4000, 30, 5)), "0");
    check("energy-cap", std::to_string(energy_next(3990, 4000, 0, 50)), "4000");
    int need = sortie_need(target, film_min, speed, cruise, film_w, esc_w, max_esc);
    check("sortie-need", std::to_string(need), "1540");
    check("launch-yes", std::to_string(launch_gate(need + reserve, need, reserve)), "1");
    check("launch-no", std::to_string(launch_gate(need + reserve - 1, need, reserve)), "0");
    check("solar-480", std::to_string(solar_at(480, 10, 390, 1080, 735, 100)), "2");

    std::vector<Leg> legs = legs_from_cfg(cfg);
    check("leg-names", join_names(legs), "out,film,home");
    check("leg-opt", join_opt(legs), "0,1,0");
    check("leg-camp", std::to_string(legs[0].dist), std::to_string(get_int(cfg, "route.camp_m")));
    check("leg-out", std::to_string(leg_need(legs[0].dist, legs[0].film, speed, cruise, film_w, esc_w, nesc)), "1700");
    check("leg-film", std::to_string(leg_need(legs[1].dist, legs[1].film, speed, cruise, film_w, esc_w, nesc)), "1600");
    check("leg-home", std::to_string(leg_need(legs[2].dist, legs[2].film, speed, cruise, film_w, esc_w, nesc)), "1070");
    check("plan-full", join_comma(day_plan(legs, 0)),
          "launch,transit,filming,return,transit,filming,return,transit,filming,return,overnight");
    check("plan-drop", join_comma(day_plan(legs, 1)),
          "launch,transit,filming,return,transit,filming,return,overnight");

    long long far = (long long)(radius + 1) * (radius + 1);
    long long edge = (long long)radius * radius;
    check("gate-order", gate_s(wind_max + 1, 0, 1, far, wind_max, need, reserve, radius), "WAIT wind");
    check("gate-battery", gate_s(wind_max, need + reserve - 1, 0, 0, wind_max, need, reserve, radius), "WAIT battery");
    check("gate-schedule", gate_s(wind_max, need + reserve, 1, 0, wind_max, need, reserve, radius), "SKIP schedule");
    check("gate-fence", gate_s(wind_max, need + reserve, 0, far, wind_max, need, reserve, radius), "SKIP fence");
    check("gate-circle", gate_s(wind_max, need + reserve, 0, edge, wind_max, need, reserve, radius), "GO ok");
    check("gate-wind-eq", gate_s(wind_max, need + reserve, 0, 0, wind_max, need, reserve, radius), "GO ok");
    check("gate-energy-eq", gate_s(0, need + reserve, 0, 0, wind_max, need, reserve, radius), "GO ok");

    std::string state = "NOMINAL";
    state = fs_request(state, "HOLD");
    check("fs-hold", state, "HOLD");
    state = fs_request(state, "NOMINAL");
    check("fs-latch-hold", state, "HOLD");
    state = fs_request(state, "RTL");
    check("fs-rtl", state, "RTL");
    state = fs_request(state, "HOLD");
    check("fs-latch-rtl", state, "RTL");
    state = fs_request(state, "LAND");
    check("fs-land", state, "LAND");
    state = fs_request(state, "NOMINAL");
    check("fs-latch-land", state, "LAND");
    state = fs_request(state, "RTL");
    check("fs-land-rtl", state, "LAND");

    check("dist2-345", std::to_string(dist2(3, 4)), "25");
    check("fence-home", std::to_string(breached(0, 0, radius)), "0");
    check("fence-edge", std::to_string(breached(radius, 0, radius)), "0");
    check("fence-out", std::to_string(breached(radius + 1, 0, radius)), "1");
    check("fence-corner", std::to_string(breached(900, 1200, radius)), "0");
    check("fence-corner-out", std::to_string(breached(900, 1201, radius)), "1");

    check("esc-idle", esc_s(0, 1, 900, conf_min, 0, max_esc, climb_m), "0,0,0,not_filming");
    check("esc-far", esc_s(1, 0, 900, conf_min, 0, max_esc, climb_m), "0,0,0,ignore");
    check("esc-low", esc_s(1, 1, conf_min - 1, conf_min, 0, max_esc, climb_m), "0,0,0,ignore");
    char climb_exp[64];
    std::snprintf(climb_exp, sizeof climb_exp, "1,%d,0,climb", climb_m);
    check("esc-climb", esc_s(1, 1, conf_min, conf_min, 0, max_esc, climb_m), climb_exp);
    std::snprintf(climb_exp, sizeof climb_exp, "3,%d,1,climb", climb_m);
    check("esc-stop", esc_s(1, 1, 700, conf_min, 2, max_esc, climb_m), climb_exp);

    check("bat-ok", battery_action(1001, 800, 200, floor), "OK");
    check("bat-eq", battery_action(1000, 800, 200, floor), "RTL");
    check("bat-floor", battery_action(floor, 800, 200, floor), "LAND");
    check("bat-crit", battery_action(floor - 1, 800, 200, floor), "LAND");

    Gear gear;
    check("gear-arm-early", gear.arm(), "REFUSE not_stowed");
    check("gear-stow", gear.request_stow(), "OK stowing");
    check("gear-arm-stowing", gear.arm(), "REFUSE not_stowed");
    check("gear-stow-done", gear.complete_stow(), "OK stowed");
    check("gear-arm", gear.arm(), "OK armed");
    gear.air = 1;
    gear.landed = 0;
    check("gear-deploy-air", gear.request_deploy(), "REFUSE not_landed");
    check("gear-stow-air", gear.request_stow(), "REFUSE airborne");
    check("gear-phase-air", gear.phase, "STOWED");
    gear.air = 0;
    gear.landed = 1;
    check("gear-deploy-ground", gear.request_deploy(), "OK deploying");
    check("gear-deploy-finish", gear.complete_deploy(), "OK deployed");

    Px4Mock px4;
    check("px4-connect", px4.connect("udp://:14540"), "REFUSE simulation_only");
    check("px4-takeoff-early", px4.takeoff(110), "REFUSE not_armed");
    check("px4-arm", px4.arm(), "ACK");
    check("px4-armed", px4.mode, "ARMED");
    check("px4-takeoff", px4.takeoff(110), "ACK");
    check("px4-goto", px4.go_to(600, 0, 110), "ACK");
    char pos[64];
    std::snprintf(pos, sizeof pos, "%d,%d,%d", px4.x, px4.y, px4.z);
    check("px4-pos", pos, "600,0,110");
    check("px4-hold", px4.hold(), "ACK");
    check("px4-hold-mode", px4.mode, "HOLD");
    check("px4-return", px4.do_return(), "ACK");
    check("px4-return-mode", px4.mode, "RETURN");
    check("px4-land", px4.land(), "ACK");
    check("px4-land-mode", px4.mode, "LAND");
    check("px4-policy", "failsafe_owner=px4 supervisor=request_only backend=mock",
          "failsafe_owner=px4 supervisor=request_only backend=mock");

    RosMock ros;
    check("ros-battery", ros.on_battery(3600, 22200), "battery 3600 22200");
    check("ros-image", ros.on_image(4, 1, 700), "image 4 1 700");
    check("ros-status", ros.publish("NOMINAL", "ok"), "status NOMINAL ok");
    check("ros-ack", ros.trigger(1), "ACK");
    check("ros-nack", ros.trigger(0), "NACK");
    check("ros-fails", std::to_string(ros.fails), "1");
    check("ros-policy", "backend=mock topics=battery,image,status,trigger",
          "backend=mock topics=battery,image,status,trigger");

    std::printf("TOTAL pass=%d/%d\n", NPASS, NTOT);
    return NPASS == NTOT ? 0 : 1;
}
