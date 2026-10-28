#pragma once
// One-sortie scenario runner. Integer minutes. Illustrative placeholders.
// Routes stay outside Grand Canyon National Park. No live MAVLink.
#include "control/battery.hpp"
#include "control/escape.hpp"
#include "control/failsafe.hpp"
#include "control/gate.hpp"
#include "control/gear.hpp"
#include "control/geofence.hpp"
#include "control/px4_mock.hpp"
#include "control/ros_mock.hpp"
#include "planner/energy.hpp"
#include "planner/mission.hpp"
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

struct ScenRow {
    const char* name;
    int wind, soc0, gust_at, esc_every, push, cam_fail_at, link_down, sag;
};

static const ScenRow SCEN_ROWS[7] = {
    {"nominal", 6, 3600, -1, 0, 0, -1, 0, 0},
    {"gust", 6, 3600, 4, 0, 0, -1, 0, 0},
    {"low_battery", 6, 2500, -1, 0, 0, -1, 0, 180},
    {"geofence_drift", 6, 3600, -1, 0, 1600, -1, 0, 0},
    {"repeated_escapes", 6, 3600, -1, 1, 0, -1, 0, 0},
    {"camera_failure", 6, 3600, -1, 0, 0, 1, 0, 0},
    {"starlink_outage", 6, 3600, -1, 0, 0, -1, 1, 0},
};

struct Rng {
    long long s;
    long long next() {
        s = (s * 1103515245LL + 12345LL) & 0x7FFFFFFFLL;
        return s >> 8;
    }
};

inline int terrain_m(int x) {
    if (x < 400) return 0;
    int height = (x - 400) / 40;
    if (height > 30) height = 30;
    return height;
}

inline void step_home(int& x, int& y, int speed) {
    int budget = speed;
    if (x > 0) {
        int step = x > budget ? budget : x;
        x -= step;
        budget -= step;
    } else if (x < 0) {
        int step = -x > budget ? budget : -x;
        x += step;
        budget -= step;
    }
    if (budget > 0 && y > 0) {
        int step = y > budget ? budget : y;
        y -= step;
    } else if (budget > 0 && y < 0) {
        int step = -y > budget ? budget : -y;
        y += step;
    }
}

inline int starts_ok(const std::string& s) { return s.size() >= 2 && s.compare(0, 2, "OK") == 0; }

struct Sortie {
    std::string name;
    int wind, gust_at, esc_every, push, cam_fail_at, link_down, sag;
    int cap, reserve, floor, cruise, film_w, esc_w, idle, uplink;
    int peak, sunrise, sunset, noon, wind_max, gust_abort, fence, min_agl_m, alt0, speed;
    int climb_m, max_esc, conf_min, stow_min, deploy_min, film_goal, cam_limit, target, up_need, giveup;
    int need;
    Rng rng;
    int soc, t;
    std::string phase, fs, reason;
    int x, y, alt, film_n, escapes, cam_fails, breaches, refuses;
    int stow_left, deploy_left, upload_credit, upload_wait;
    std::string upload;
    int pushed, illegal_done, arm_after_stow, deploy_viol, return_sent;
    int min_soc, min_agl, steps;
    long long rng_acc;
    std::vector<std::string> events, rows;
    Gear gear;
    Px4Mock px4;
    RosMock ros;
    char buf[512];

    Sortie(const Tree& cfg, int sid) {
        const ScenRow& row = SCEN_ROWS[sid];
        name = row.name;
        wind = row.wind;
        gust_at = row.gust_at;
        esc_every = row.esc_every;
        push = row.push;
        cam_fail_at = row.cam_fail_at;
        link_down = row.link_down;
        sag = row.sag;
        cap = get_int(cfg, "energy_dwh.capacity");
        reserve = get_int(cfg, "energy_dwh.launch_reserve");
        floor = get_int(cfg, "energy_dwh.flight_floor");
        cruise = get_int(cfg, "draw_dwh_per_min.cruise");
        film_w = get_int(cfg, "draw_dwh_per_min.film");
        esc_w = get_int(cfg, "draw_dwh_per_min.escape");
        idle = get_int(cfg, "draw_dwh_per_min.idle");
        uplink = get_int(cfg, "draw_dwh_per_min.uplink");
        peak = get_int(cfg, "solar.peak_dwh_per_min");
        sunrise = get_int(cfg, "solar.sunrise_min");
        sunset = get_int(cfg, "solar.sunset_min");
        noon = get_int(cfg, "solar.noon_min");
        wind_max = get_int(cfg, "limits.wind_max_ms");
        gust_abort = get_int(cfg, "limits.gust_abort_ms");
        fence = get_int(cfg, "limits.geofence_radius_m");
        min_agl_m = get_int(cfg, "limits.min_agl_m");
        alt0 = get_int(cfg, "limits.alt_m");
        speed = get_int(cfg, "limits.speed_m_per_min");
        climb_m = get_int(cfg, "escape.climb_m");
        max_esc = get_int(cfg, "escape.max_count");
        conf_min = get_int(cfg, "escape.conf_min_permille");
        stow_min = get_int(cfg, "gear.stow_min");
        deploy_min = get_int(cfg, "gear.deploy_min");
        film_goal = get_int(cfg, "film.minutes");
        cam_limit = get_int(cfg, "film.cam_fail_limit");
        target = get_int(cfg, "film.target_m");
        up_need = get_int(cfg, "upload.need_min");
        giveup = get_int(cfg, "upload.outage_giveup_min");
        need = sortie_need(target, film_goal, speed, cruise, film_w, esc_w, max_esc);
        rng.s = 2026 + sid;
        soc = row.soc0;
        t = 480;
        phase = "WAIT";
        fs = "NOMINAL";
        reason = "";
        x = y = alt = 0;
        film_n = escapes = cam_fails = breaches = refuses = 0;
        stow_left = deploy_left = upload_credit = upload_wait = 0;
        upload = "none";
        pushed = illegal_done = arm_after_stow = deploy_viol = return_sent = 0;
        min_soc = row.soc0;
        min_agl = 9999;
        steps = 0;
        rng_acc = 0;
    }

    void ev(const std::string& msg) {
        std::snprintf(buf, sizeof buf, "  t=%04d %s", t, msg.c_str());
        events.push_back(buf);
    }

    void commit(int draw, const std::string& label) {
        int gain = 0;
        if (label == "UPLOAD") gain = solar_at(t, peak, sunrise, sunset, noon, 100);
        soc = energy_next(soc, cap, draw, gain);
        if (soc < min_soc) min_soc = soc;
        if (label == "TRANSIT" || label == "FILM" || label == "RETURN") {
            int agl = alt - terrain_m(x);
            if (agl < min_agl) min_agl = agl;
        }
        std::snprintf(buf, sizeof buf, "%s,%d,%s,%s,%d,%d,%d,%d,%d,%d,%s",
                      name.c_str(), t, label.c_str(), fs.c_str(), draw, gain, soc, x, y, alt,
                      gear.phase.c_str());
        rows.push_back(buf);
        rng_acc = (rng_acc + rng.next()) & 0x7FFFFFFFLL;
        t += 1;
    }

    int ok() const {
        int good = phase == "DONE" && arm_after_stow && refuses >= 1;
        good = good && deploy_viol == 0 && x == 0 && y == 0;
        good = good && gear.phase == "DEPLOYED" && gear.air == 0 && gear.landed == 1;
        good = good && px4.requests == 5 && px4.mode == "LAND";
        good = good && min_agl >= min_agl_m && min_agl < 5000;
        if (name == "nominal") {
            good = good && reason == "film_done" && fs == "NOMINAL";
            good = good && film_n == 8 && escapes == 0 && cam_fails == 0;
            good = good && upload == "done" && breaches == 0;
        } else if (name == "gust") {
            good = good && reason == "gust" && fs == "RTL" && film_n == 4 && upload == "done";
        } else if (name == "low_battery") {
            good = good && reason == "battery" && fs == "RTL" && film_n == 5 && upload == "done";
        } else if (name == "geofence_drift") {
            good = good && reason == "geofence" && fs == "RTL";
            good = good && breaches >= 1 && film_n == 0 && upload == "done";
        } else if (name == "repeated_escapes") {
            good = good && reason == "escape" && fs == "RTL";
            good = good && escapes == 3 && film_n == 3 && upload == "done";
        } else if (name == "camera_failure") {
            good = good && reason == "camera" && fs == "RTL";
            good = good && cam_fails == 3 && film_n == 3 && upload == "done";
        } else if (name == "starlink_outage") {
            good = good && reason == "film_done" && fs == "NOMINAL";
            good = good && upload == "deferred" && film_n == 8;
        } else {
            good = 0;
        }
        return good ? 1 : 0;
    }

    std::string result_line() {
        int passed = ok();
        std::snprintf(buf, sizeof buf,
                      "  RESULT %s fs=%s reason=%s film=%d esc=%d fails=%d breach=%d upload=%s min_soc=%d min_agl=%d end=%d,%d gear=%s refuse=%d px4=%d rng=%lld %s",
                      name.c_str(), fs.c_str(), reason.c_str(), film_n, escapes, cam_fails, breaches,
                      upload.c_str(), min_soc, min_agl, x, y, gear.phase.c_str(), refuses, px4.requests,
                      rng_acc, passed ? "PASS" : "FAIL");
        return buf;
    }

    void on_wait() {
        Decision d = go_nogo(wind, wind_max, soc, need, reserve, 0, dist2(x, y), fence);
        std::snprintf(buf, sizeof buf, "%s %s soc=%d", d.action.c_str(), d.why.c_str(), soc);
        ev(buf);
        if (d.action == "GO") {
            std::string got = gear.request_stow();
            if (!starts_ok(got)) {
                refuses += 1;
                ev("STOW " + got);
                phase = "DONE";
            } else {
                phase = "STOW";
                stow_left = stow_min;
            }
        } else if (d.action == "SKIP") {
            reason = d.why;
            phase = "DONE";
        } else {
            commit(idle, "WAIT");
            if (t > 600) {
                reason = d.why;
                ev("WAIT timeout");
                phase = "DONE";
            }
        }
    }

    void on_stow() {
        stow_left -= 1;
        if (stow_left == 0) {
            ev("STOW " + gear.complete_stow());
            std::string arm_s = gear.arm();
            if (!starts_ok(arm_s)) {
                refuses += 1;
                ev("ARM " + arm_s);
                phase = "DONE";
            } else {
                arm_after_stow = 1;
                std::string ack = px4.arm();
                std::snprintf(buf, sizeof buf, "ARM %s mode=%s", ack.c_str(), px4.mode.c_str());
                ev(buf);
                ack = px4.takeoff(alt0);
                alt = alt0;
                gear.air = 1;
                gear.landed = 0;
                std::snprintf(buf, sizeof buf, "TAKEOFF %s alt=%d", ack.c_str(), alt);
                ev(buf);
                ack = px4.go_to(target, 0, alt0);
                std::snprintf(buf, sizeof buf, "GOTO %s x=%d y=%d z=%d", ack.c_str(), target, 0, alt0);
                ev(buf);
                phase = "TRANSIT";
            }
        }
        commit(idle, "STOW");
    }

    void on_transit() {
        if (!illegal_done && gear.air) {
            std::string got = gear.request_deploy();
            illegal_done = 1;
            if (starts_ok(got)) deploy_viol += 1;
            else refuses += 1;
            ev("DEPLOY " + got);
        }
        int delta = target - x;
        if (delta < 0) delta = 0;
        if (delta > speed) delta = speed;
        x += delta;
        if (push && !pushed) {
            y += push;
            pushed = 1;
        }
        if (breached(x, y, fence)) {
            breaches += 1;
            fs = fs_request(fs, "RTL");
            reason = "geofence";
            std::snprintf(buf, sizeof buf, "GEOFENCE x=%d y=%d -> RTL", x, y);
            ev(buf);
            phase = "RETURN";
        } else if (x >= target) {
            phase = "FILM";
        }
        commit(cruise, "TRANSIT");
    }

    void on_film() {
        int ret_cost = (iabs_i(x) / speed + 1) * cruise + cruise;
        std::string action = battery_action(soc, reserve, ret_cost, floor);
        if (action != "OK") {
            fs = fs_request(fs, action);
            reason = "battery";
            std::snprintf(buf, sizeof buf, "BATTERY %s soc=%d cost=%d", action.c_str(), soc, ret_cost);
            ev(buf);
            phase = action == "LAND" ? "LAND" : "RETURN";
            return;
        }
        if (film_n == 0) {
            std::snprintf(buf, sizeof buf, "FILM start x=%d", x);
            ev(buf);
        }
        film_n += 1;
        int close = (esc_every && film_n % esc_every == 0) ? 1 : 0;
        int conf = close ? 700 : 100;
        ros.on_battery(soc, 22200);
        ros.on_image(film_n, close, conf);
        Esc esc = escape_check(1, close, conf, conf_min, escapes, max_esc, climb_m);
        escapes = esc.escapes;
        int draw;
        if (esc.climb) {
            alt += esc.climb;
            draw = esc_w + sag;
            std::snprintf(buf, sizeof buf, "ESCAPE #%d climb +%d m", escapes, esc.climb);
            ev(buf);
        } else {
            draw = film_w + sag;
        }
        int ack_bit = (cam_fail_at >= 0 && film_n >= cam_fail_at) ? 0 : 1;
        std::string trig = ros.trigger(ack_bit);
        if (trig != "ACK") {
            cam_fails += 1;
            std::snprintf(buf, sizeof buf, "TRIGGER %s fails=%d", trig.c_str(), cam_fails);
            ev(buf);
        }
        std::string end_reason;
        int gust_wind = wind;
        if (gust_at == film_n) gust_wind = gust_abort + 2;
        if (esc.stop) {
            fs = fs_request(fs, "RTL");
            end_reason = "escape";
        } else if (cam_fails >= cam_limit) {
            fs = fs_request(fs, "RTL");
            end_reason = "camera";
        } else if (gust_wind > gust_abort) {
            fs = fs_request(fs, "RTL");
            end_reason = "gust";
        } else if (film_n >= film_goal) {
            end_reason = "film_done";
        }
        if (!end_reason.empty()) {
            reason = end_reason;
            std::snprintf(buf, sizeof buf, "END %s film=%d fs=%s", end_reason.c_str(), film_n, fs.c_str());
            ev(buf);
            phase = "RETURN";
        }
        ros.publish(fs, reason.empty() ? "film" : reason);
        commit(draw, "FILM");
    }

    void on_return() {
        if (!return_sent) {
            std::string ack = px4.do_return();
            return_sent = 1;
            std::snprintf(buf, sizeof buf, "RETURN %s reason=%s", ack.c_str(), reason.c_str());
            ev(buf);
        }
        step_home(x, y, speed);
        if (x == 0 && y == 0) {
            ev("HOME");
            phase = "LAND";
        }
        commit(cruise, "RETURN");
    }

    void on_land() {
        std::string ack = px4.land();
        alt = 0;
        gear.air = 0;
        gear.landed = 1;
        std::snprintf(buf, sizeof buf, "LAND %s mode=%s", ack.c_str(), px4.mode.c_str());
        ev(buf);
        std::string got = gear.request_deploy();
        ev("DEPLOY " + got);
        if (!starts_ok(got)) {
            refuses += 1;
            deploy_viol += 1;
            phase = "DONE";
        } else {
            deploy_left = deploy_min;
            phase = "DEPLOY";
        }
        commit(cruise, "LAND");
    }

    void on_deploy() {
        deploy_left -= 1;
        if (deploy_left == 0) {
            std::string got = gear.complete_deploy();
            ev("DEPLOY " + got);
            if (!starts_ok(got)) {
                refuses += 1;
                phase = "DONE";
            } else {
                phase = "UPLOAD";
            }
        }
        commit(idle, "DEPLOY");
    }

    void on_upload() {
        if (link_down) {
            upload_wait += 1;
            ros.publish(fs, "uplink_down");
            if (upload_wait >= giveup) {
                upload = "deferred";
                ev("UPLOAD deferred");
                phase = "DONE";
            }
            commit(idle, "UPLOAD");
        } else {
            upload_credit += 1;
            ros.publish(fs, "uplink");
            if (upload_credit >= up_need) {
                upload = "done";
                ev("UPLOAD done");
                phase = "DONE";
            }
            commit(idle + uplink, "UPLOAD");
        }
    }

    int run(std::string& line) {
        while (steps < 80 && phase != "DONE") {
            steps += 1;
            if (phase == "WAIT") on_wait();
            else if (phase == "STOW") on_stow();
            else if (phase == "TRANSIT") on_transit();
            else if (phase == "FILM") on_film();
            else if (phase == "RETURN") on_return();
            else if (phase == "LAND") on_land();
            else if (phase == "DEPLOY") on_deploy();
            else if (phase == "UPLOAD") on_upload();
            else phase = "DONE";
        }
        line = result_line();
        return ok();
    }
};

inline int run_stack(const std::string& csv_path, const std::string& cfg_path) {
    Tree cfg = load_file(cfg_path);
    std::vector<std::string> lines;
    lines.push_back("AEO-9 planning stack (illustrative placeholders, simulation only, outside Grand Canyon NP)");
    char buf[256];
    std::snprintf(buf, sizeof buf, "cfg wind_max_ms=%d reserve_dwh=%d geofence_radius_m=%d min_agl_m=%d",
                  get_int(cfg, "limits.wind_max_ms"), get_int(cfg, "energy_dwh.launch_reserve"),
                  get_int(cfg, "limits.geofence_radius_m"), get_int(cfg, "limits.min_agl_m"));
    lines.push_back(buf);
    std::vector<std::string> rows;
    rows.push_back("scenario,t,phase,failsafe,draw_dwh,gain_dwh,soc_dwh,x_m,y_m,alt_m,gear");
    int npass = 0;
    for (int sid = 0; sid < 7; ++sid) {
        Sortie sortie(cfg, sid);
        std::string result;
        int passed = sortie.run(result);
        std::snprintf(buf, sizeof buf, "scenario %d %s", sid, SCEN_ROWS[sid].name);
        lines.push_back(buf);
        for (std::size_t i = 0; i < sortie.events.size(); ++i) lines.push_back(sortie.events[i]);
        lines.push_back(result);
        for (std::size_t i = 0; i < sortie.rows.size(); ++i) rows.push_back(sortie.rows[i]);
        npass += passed;
    }
    std::snprintf(buf, sizeof buf, "TOTAL pass=%d/7", npass);
    lines.push_back(buf);
    for (std::size_t i = 0; i < lines.size(); ++i) std::printf("%s\n", lines[i].c_str());
    std::ofstream out(csv_path.c_str());
    for (std::size_t i = 0; i < rows.size(); ++i) out << rows[i] << "\n";
    return npass == 7 ? 0 : 1;
}
