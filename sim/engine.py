"""One-sortie scenario runner. Integer minutes. Illustrative placeholders.

Routes stay outside Grand Canyon National Park. No live MAVLink.
"""

from planner.config_loader import get_int
from planner.energy import energy_next, solar_at
from planner.mission import sortie_need
from control.gate import go_nogo
from control.failsafe import fs_request
from control.geofence import breached, dist2
from control.escape import escape_check
from control.battery import battery_action
from control.gear import Gear
from control.px4_mock import Px4Mock
from control.ros_mock import RosMock

# name, wind, soc0, gust_at, esc_every, push, cam_fail_at, link_down, sag
SCEN = (
    ("nominal", 6, 3600, -1, 0, 0, -1, 0, 0),
    ("gust", 6, 3600, 4, 0, 0, -1, 0, 0),
    ("low_battery", 6, 2500, -1, 0, 0, -1, 0, 180),
    ("geofence_drift", 6, 3600, -1, 0, 1600, -1, 0, 0),
    ("repeated_escapes", 6, 3600, -1, 1, 0, -1, 0, 0),
    ("camera_failure", 6, 3600, -1, 0, 0, 1, 0, 0),
    ("starlink_outage", 6, 3600, -1, 0, 0, -1, 1, 0),
)


class Rng(object):
    def __init__(self, seed):
        self.s = seed

    def next(self):
        self.s = (self.s * 1103515245 + 12345) & 0x7FFFFFFF
        return self.s >> 8


def terrain(x):
    if x < 400:
        return 0
    height = (x - 400) // 40
    if height > 30:
        height = 30
    return height


def step_home(x, y, speed):
    budget = speed
    if x > 0:
        step = budget if x > budget else x
        x -= step
        budget -= step
    elif x < 0:
        step = budget if -x > budget else -x
        x += step
        budget -= step
    if budget > 0 and y > 0:
        step = budget if y > budget else y
        y -= step
    elif budget > 0 and y < 0:
        step = budget if -y > budget else -y
        y += step
    return x, y


class Sortie(object):
    def __init__(self, cfg, sid):
        name, wind, soc0, gust_at, esc_every, push, cam_fail_at, link_down, sag = SCEN[sid]
        self.name = name
        self.wind = wind
        self.gust_at = gust_at
        self.esc_every = esc_every
        self.push = push
        self.cam_fail_at = cam_fail_at
        self.link_down = link_down
        self.sag = sag
        self.cap = get_int(cfg, "energy_dwh.capacity")
        self.reserve = get_int(cfg, "energy_dwh.launch_reserve")
        self.floor = get_int(cfg, "energy_dwh.flight_floor")
        self.cruise = get_int(cfg, "draw_dwh_per_min.cruise")
        self.film_w = get_int(cfg, "draw_dwh_per_min.film")
        self.esc_w = get_int(cfg, "draw_dwh_per_min.escape")
        self.idle = get_int(cfg, "draw_dwh_per_min.idle")
        self.uplink = get_int(cfg, "draw_dwh_per_min.uplink")
        self.peak = get_int(cfg, "solar.peak_dwh_per_min")
        self.sunrise = get_int(cfg, "solar.sunrise_min")
        self.sunset = get_int(cfg, "solar.sunset_min")
        self.noon = get_int(cfg, "solar.noon_min")
        self.wind_max = get_int(cfg, "limits.wind_max_ms")
        self.gust_abort = get_int(cfg, "limits.gust_abort_ms")
        self.fence = get_int(cfg, "limits.geofence_radius_m")
        self.min_agl_m = get_int(cfg, "limits.min_agl_m")
        self.alt0 = get_int(cfg, "limits.alt_m")
        self.speed = get_int(cfg, "limits.speed_m_per_min")
        self.climb_m = get_int(cfg, "escape.climb_m")
        self.max_esc = get_int(cfg, "escape.max_count")
        self.conf_min = get_int(cfg, "escape.conf_min_permille")
        self.stow_min = get_int(cfg, "gear.stow_min")
        self.deploy_min = get_int(cfg, "gear.deploy_min")
        self.film_goal = get_int(cfg, "film.minutes")
        self.cam_limit = get_int(cfg, "film.cam_fail_limit")
        self.target = get_int(cfg, "film.target_m")
        self.up_need = get_int(cfg, "upload.need_min")
        self.giveup = get_int(cfg, "upload.outage_giveup_min")
        self.need = sortie_need(
            self.target, self.film_goal, self.speed, self.cruise,
            self.film_w, self.esc_w, self.max_esc)
        self.rng = Rng(2026 + sid)
        self.soc = soc0
        self.t = 480
        self.phase = "WAIT"
        self.fs = "NOMINAL"
        self.reason = ""
        self.x = 0
        self.y = 0
        self.alt = 0
        self.film_n = 0
        self.escapes = 0
        self.cam_fails = 0
        self.breaches = 0
        self.refuses = 0
        self.stow_left = 0
        self.deploy_left = 0
        self.upload_credit = 0
        self.upload_wait = 0
        self.upload = "none"
        self.pushed = 0
        self.illegal_done = 0
        self.arm_after_stow = 0
        self.deploy_viol = 0
        self.return_sent = 0
        self.min_soc = soc0
        self.min_agl = 9999
        self.rng_acc = 0
        self.steps = 0
        self.events = []
        self.rows = []
        self.gear = Gear()
        self.px4 = Px4Mock()
        self.ros = RosMock()

    def ev(self, msg):
        self.events.append("  t=%04d %s" % (self.t, msg))

    def commit(self, draw, label):
        gain = 0
        if label == "UPLOAD":
            gain = solar_at(self.t, self.peak, self.sunrise, self.sunset, self.noon, 100)
        self.soc = energy_next(self.soc, self.cap, draw, gain)
        if self.soc < self.min_soc:
            self.min_soc = self.soc
        if label == "TRANSIT" or label == "FILM" or label == "RETURN":
            agl = self.alt - terrain(self.x)
            if agl < self.min_agl:
                self.min_agl = agl
        self.rows.append("%s,%d,%s,%s,%d,%d,%d,%d,%d,%d,%s" % (
            self.name, self.t, label, self.fs, draw, gain, self.soc,
            self.x, self.y, self.alt, self.gear.phase))
        self.rng_acc = (self.rng_acc + self.rng.next()) & 0x7FFFFFFF
        self.t += 1

    def ok(self):
        good = self.phase == "DONE" and self.arm_after_stow and self.refuses >= 1
        good = good and self.deploy_viol == 0 and self.x == 0 and self.y == 0
        good = good and self.gear.phase == "DEPLOYED" and self.gear.air == 0 and self.gear.landed == 1
        good = good and self.px4.requests == 5 and self.px4.mode == "LAND"
        good = good and self.min_agl >= self.min_agl_m and self.min_agl < 5000
        name = self.name
        if name == "nominal":
            good = good and self.reason == "film_done" and self.fs == "NOMINAL"
            good = good and self.film_n == 8 and self.escapes == 0 and self.cam_fails == 0
            good = good and self.upload == "done" and self.breaches == 0
        elif name == "gust":
            good = good and self.reason == "gust" and self.fs == "RTL" and self.film_n == 4
            good = good and self.upload == "done"
        elif name == "low_battery":
            good = good and self.reason == "battery" and self.fs == "RTL" and self.film_n == 5
            good = good and self.upload == "done"
        elif name == "geofence_drift":
            good = good and self.reason == "geofence" and self.fs == "RTL"
            good = good and self.breaches >= 1 and self.film_n == 0 and self.upload == "done"
        elif name == "repeated_escapes":
            good = good and self.reason == "escape" and self.fs == "RTL"
            good = good and self.escapes == 3 and self.film_n == 3 and self.upload == "done"
        elif name == "camera_failure":
            good = good and self.reason == "camera" and self.fs == "RTL"
            good = good and self.cam_fails == 3 and self.film_n == 3 and self.upload == "done"
        elif name == "starlink_outage":
            good = good and self.reason == "film_done" and self.fs == "NOMINAL"
            good = good and self.upload == "deferred" and self.film_n == 8
        else:
            good = False
        return 1 if good else 0

    def result(self):
        passed = self.ok()
        line = "  RESULT %s fs=%s reason=%s film=%d esc=%d fails=%d breach=%d upload=%s min_soc=%d min_agl=%d end=%d,%d gear=%s refuse=%d px4=%d rng=%d %s" % (
            self.name, self.fs, self.reason, self.film_n, self.escapes, self.cam_fails,
            self.breaches, self.upload, self.min_soc, self.min_agl, self.x, self.y,
            self.gear.phase, self.refuses, self.px4.requests, self.rng_acc,
            "PASS" if passed else "FAIL")
        return line, passed

    def on_wait(self):
        decision, why = go_nogo(
            self.wind, self.wind_max, self.soc, self.need, self.reserve, 0,
            dist2(self.x, self.y), self.fence)
        self.ev("%s %s soc=%d" % (decision, why, self.soc))
        if decision == "GO":
            got = self.gear.request_stow()
            if got[:2] != "OK":
                self.refuses += 1
                self.ev("STOW %s" % got)
                self.phase = "DONE"
            else:
                self.phase = "STOW"
                self.stow_left = self.stow_min
        elif decision == "SKIP":
            self.reason = why
            self.phase = "DONE"
        else:
            self.commit(self.idle, "WAIT")
            if self.t > 600:
                self.reason = why
                self.ev("WAIT timeout")
                self.phase = "DONE"

    def on_stow(self):
        self.stow_left -= 1
        if self.stow_left == 0:
            self.ev("STOW %s" % self.gear.complete_stow())
            arm = self.gear.arm()
            if arm[:2] != "OK":
                self.refuses += 1
                self.ev("ARM %s" % arm)
                self.phase = "DONE"
            else:
                self.arm_after_stow = 1
                ack = self.px4.arm()
                self.ev("ARM %s mode=%s" % (ack, self.px4.mode))
                ack = self.px4.takeoff(self.alt0)
                self.alt = self.alt0
                self.gear.air = 1
                self.gear.landed = 0
                self.ev("TAKEOFF %s alt=%d" % (ack, self.alt))
                ack = self.px4.goto(self.target, 0, self.alt0)
                self.ev("GOTO %s x=%d y=%d z=%d" % (ack, self.target, 0, self.alt0))
                self.phase = "TRANSIT"
        self.commit(self.idle, "STOW")

    def on_transit(self):
        if not self.illegal_done and self.gear.air:
            got = self.gear.request_deploy()
            self.illegal_done = 1
            if got[:2] == "OK":
                self.deploy_viol += 1
            else:
                self.refuses += 1
            self.ev("DEPLOY %s" % got)
        delta = self.target - self.x
        if delta < 0:
            delta = 0
        if delta > self.speed:
            delta = self.speed
        self.x += delta
        if self.push and not self.pushed:
            self.y += self.push
            self.pushed = 1
        if breached(self.x, self.y, self.fence):
            self.breaches += 1
            self.fs = fs_request(self.fs, "RTL")
            self.reason = "geofence"
            self.ev("GEOFENCE x=%d y=%d -> RTL" % (self.x, self.y))
            self.phase = "RETURN"
        elif self.x >= self.target:
            self.phase = "FILM"
        self.commit(self.cruise, "TRANSIT")

    def on_film(self):
        # Cruise home plus the landing minute. Illustrative, not a measured draw.
        ret_cost = (abs(self.x) // self.speed + 1) * self.cruise + self.cruise
        action = battery_action(self.soc, self.reserve, ret_cost, self.floor)
        if action != "OK":
            self.fs = fs_request(self.fs, action)
            self.reason = "battery"
            self.ev("BATTERY %s soc=%d cost=%d" % (action, self.soc, ret_cost))
            self.phase = "LAND" if action == "LAND" else "RETURN"
            return
        if self.film_n == 0:
            self.ev("FILM start x=%d" % self.x)
        self.film_n += 1
        close = 1 if self.esc_every and self.film_n % self.esc_every == 0 else 0
        conf = 700 if close else 100
        self.ros.on_battery(self.soc, 22200)
        self.ros.on_image(self.film_n, close, conf)
        escapes, climb, stop, _why = escape_check(
            1, close, conf, self.conf_min, self.escapes, self.max_esc, self.climb_m)
        self.escapes = escapes
        if climb:
            self.alt += climb
            draw = self.esc_w + self.sag
            self.ev("ESCAPE #%d climb +%d m" % (self.escapes, climb))
        else:
            draw = self.film_w + self.sag
        ack_bit = 0 if self.cam_fail_at >= 0 and self.film_n >= self.cam_fail_at else 1
        trig = self.ros.trigger(ack_bit)
        if trig != "ACK":
            self.cam_fails += 1
            self.ev("TRIGGER %s fails=%d" % (trig, self.cam_fails))
        end_reason = ""
        gust_wind = self.wind
        if self.gust_at == self.film_n:
            gust_wind = self.gust_abort + 2
        if stop:
            self.fs = fs_request(self.fs, "RTL")
            end_reason = "escape"
        elif self.cam_fails >= self.cam_limit:
            self.fs = fs_request(self.fs, "RTL")
            end_reason = "camera"
        elif gust_wind > self.gust_abort:
            self.fs = fs_request(self.fs, "RTL")
            end_reason = "gust"
        elif self.film_n >= self.film_goal:
            end_reason = "film_done"
        if end_reason:
            self.reason = end_reason
            self.ev("END %s film=%d fs=%s" % (end_reason, self.film_n, self.fs))
            self.phase = "RETURN"
        self.ros.publish(self.fs, self.reason if self.reason else "film")
        self.commit(draw, "FILM")

    def on_return(self):
        if not self.return_sent:
            ack = self.px4.do_return()
            self.return_sent = 1
            self.ev("RETURN %s reason=%s" % (ack, self.reason))
        self.x, self.y = step_home(self.x, self.y, self.speed)
        if self.x == 0 and self.y == 0:
            self.ev("HOME")
            self.phase = "LAND"
        self.commit(self.cruise, "RETURN")

    def on_land(self):
        ack = self.px4.land()
        self.alt = 0
        self.gear.air = 0
        self.gear.landed = 1
        self.ev("LAND %s mode=%s" % (ack, self.px4.mode))
        got = self.gear.request_deploy()
        self.ev("DEPLOY %s" % got)
        if got[:2] != "OK":
            self.refuses += 1
            self.deploy_viol += 1
            self.phase = "DONE"
        else:
            self.deploy_left = self.deploy_min
            self.phase = "DEPLOY"
        self.commit(self.cruise, "LAND")

    def on_deploy(self):
        self.deploy_left -= 1
        if self.deploy_left == 0:
            got = self.gear.complete_deploy()
            self.ev("DEPLOY %s" % got)
            if got[:2] != "OK":
                self.refuses += 1
                self.phase = "DONE"
            else:
                self.phase = "UPLOAD"
        self.commit(self.idle, "DEPLOY")

    def on_upload(self):
        if self.link_down:
            self.upload_wait += 1
            self.ros.publish(self.fs, "uplink_down")
            if self.upload_wait >= self.giveup:
                self.upload = "deferred"
                self.ev("UPLOAD deferred")
                self.phase = "DONE"
            self.commit(self.idle, "UPLOAD")
        else:
            self.upload_credit += 1
            self.ros.publish(self.fs, "uplink")
            if self.upload_credit >= self.up_need:
                self.upload = "done"
                self.ev("UPLOAD done")
                self.phase = "DONE"
            self.commit(self.idle + self.uplink, "UPLOAD")

    def run(self):
        while self.steps < 80 and self.phase != "DONE":
            self.steps += 1
            if self.phase == "WAIT":
                self.on_wait()
            elif self.phase == "STOW":
                self.on_stow()
            elif self.phase == "TRANSIT":
                self.on_transit()
            elif self.phase == "FILM":
                self.on_film()
            elif self.phase == "RETURN":
                self.on_return()
            elif self.phase == "LAND":
                self.on_land()
            elif self.phase == "DEPLOY":
                self.on_deploy()
            elif self.phase == "UPLOAD":
                self.on_upload()
            else:
                self.phase = "DONE"
        return self.result()


def run_all(cfg):
    lines = [
        "AEO-9 planning stack (illustrative placeholders, simulation only, outside Grand Canyon NP)",
        "cfg wind_max_ms=%d reserve_dwh=%d geofence_radius_m=%d min_agl_m=%d" % (
            get_int(cfg, "limits.wind_max_ms"),
            get_int(cfg, "energy_dwh.launch_reserve"),
            get_int(cfg, "limits.geofence_radius_m"),
            get_int(cfg, "limits.min_agl_m")),
    ]
    rows = ["scenario,t,phase,failsafe,draw_dwh,gain_dwh,soc_dwh,x_m,y_m,alt_m,gear"]
    npass = 0
    for sid in range(len(SCEN)):
        sortie = Sortie(cfg, sid)
        result, passed = sortie.run()
        lines.append("scenario %d %s" % (sid, SCEN[sid][0]))
        lines.extend(sortie.events)
        lines.append(result)
        rows.extend(sortie.rows)
        npass += passed
    lines.append("TOTAL pass=%d/%d" % (npass, len(SCEN)))
    return lines, rows, npass, len(SCEN)
