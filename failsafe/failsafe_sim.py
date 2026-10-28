# Deterministic failsafe state-machine test (integer math only, seeded LCG).
# Units: mm, mm per tick (tick = 20 ms, 50 Hz), battery in 0.01 %.
NAMES = ["CRUISE", "ESCAPE_CLIMB", "RTL_CLIMB", "RTL_CRUISE", "RTL_DESCEND",
         "LAND", "LANDED", "PANELS", "DONE"]
CRUISE, ESCAPE, RTL_CLIMB, RTL_CRUISE, RTL_DESC, LAND, LANDED, PANELS, DONE = range(9)
FENCE = 120000; WP = 100000; CRUISE_ALT = 30000; ESC_CLIMB = 15000
LOW = 2500; CRIT = 1500; GPS_MS = 2000; LINK_MS = 3000; STALE_MS = 200
STALE_RTL_MS = 4000; CONFIRM = 3; CONF_MIN = 600; MISSION_END = 3000
COVER_TICKS = 150; PANEL_TICKS = 250; LAND_ALT = 300; GEAR_ALT = 5000
MAX_TICKS = 8000


class Rng:
    def __init__(self, seed):
        self.s = seed

    def next(self):
        self.s = (self.s * 1103515245 + 12345) & 0x7FFFFFFF
        return self.s >> 8


# id, name, events
SCEN = ["nominal", "animal", "false_positive", "geofence", "low_battery",
        "gps_loss", "link_loss", "dropout_short", "dropout_long", "wind_gust"]


def run(sid):
    name = SCEN[sid]
    rng = Rng(2026 + sid)
    st = CRUISE; d = 0; alt = CRUISE_ALT
    bat0 = 3200 if name == "low_battery" else 9000
    rate = 20 if name == "low_battery" else 2
    last_frame = 0; cnt = 0; gps_bad = 0; link_bad = 0
    esc_target = 0; rtl_alt = 0; timer = 0
    log = []; first_reason = ""; first_tick = -1
    max_d = 0; max_alt = alt; det_hold = 0; viol = 0
    gear_tick = -1; cover_tick = -1; panel_tick = -1; done_tick = -1
    visited = set([CRUISE]); esc_tick = -1

    def go(i, new, why):
        nonlocal st, first_reason, first_tick
        log.append("  t=%05d %s->%s %s" % (i, NAMES[st], NAMES[new], why))
        if first_tick < 0 and why not in ("", "START"):
            first_reason = why; first_tick = i
        st = new; visited.add(new)

    for i in range(MAX_TICKS):
        # ---- scripted inputs
        gps_ok = not (name == "gps_loss" and i >= 2000)
        link_ok = not (name == "link_loss" and 2000 <= i < 2500)
        animal = (name == "animal" and 1000 <= i < 1500) or \
                 (name == "dropout_short" and 1550 <= i < 2000)
        drop = (name == "dropout_short" and 1500 <= i < 1625) or \
               (name == "dropout_long" and 1500 <= i < 1800)
        if i % 5 == 0 and not drop:
            r = rng.next()
            conf = (700 + r % 250) if animal else (r % 500)
            if name == "false_positive" and i == 750:
                conf = 650
            cnt = cnt + 1 if conf >= CONF_MIN else 0
            last_frame = i
        g = 0
        if name == "wind_gust" and 1000 <= i < 2000:
            g = rng.next() % 201 - 100
        bat = bat0 - i * rate // 50
        stale = (i - last_frame) * 20
        gps_bad = 0 if gps_ok else gps_bad + 20
        link_bad = 0 if link_ok else link_bad + 20
        # ---- supervisor
        if st in (CRUISE, ESCAPE, RTL_CLIMB, RTL_CRUISE):
            if gps_bad >= GPS_MS:
                go(i, LAND, "GPS_LOSS")
            elif bat <= CRIT:
                go(i, LAND, "BATT_CRIT")
            elif st in (CRUISE, ESCAPE):
                why = ""
                if d > FENCE: why = "GEOFENCE"
                elif link_bad >= LINK_MS: why = "LINK_LOSS"
                elif bat <= LOW: why = "BATT_LOW"
                elif stale >= STALE_RTL_MS: why = "DET_STALE"
                elif i >= MISSION_END: why = "MISSION_END"
                if why:
                    rtl_alt = max(alt, CRUISE_ALT)
                    go(i, RTL_CLIMB, why)
                elif st == CRUISE:
                    if stale > STALE_MS:
                        det_hold += 1
                    elif cnt >= CONFIRM:
                        esc_target = alt + ESC_CLIMB; esc_tick = i
                        go(i, ESCAPE, "ANIMAL")
        # ---- dynamics
        if st in (CRUISE, ESCAPE, RTL_CLIMB, RTL_CRUISE) or st == RTL_DESC or st == LAND:
            d += g
        if st == CRUISE:
            if d < WP: d += 80
            if name == "geofence" and i >= 1500: d += 120
        elif st == ESCAPE:
            alt += 40
            if alt >= esc_target:
                rtl_alt = alt; go(i, RTL_CLIMB, "ESCAPE_DONE")
        elif st == RTL_CLIMB:
            if alt < rtl_alt: alt += 40
            else: go(i, RTL_CRUISE, "")
        elif st == RTL_CRUISE:
            d -= 100
            if d <= 0: d = 0; go(i, RTL_DESC, "")
        elif st in (RTL_DESC, LAND):
            alt -= 30
            if alt <= GEAR_ALT and gear_tick < 0: gear_tick = i
            if alt <= LAND_ALT:
                if st == LAND and alt > LAND_ALT: viol += 1
                alt = 0; timer = 0; go(i, LANDED, "")
                cover_tick = i
        elif st == LANDED:
            timer += 1
            if timer >= COVER_TICKS:
                timer = 0; go(i, PANELS, ""); panel_tick = i
        elif st == PANELS:
            timer += 1
            if timer >= PANEL_TICKS:
                go(i, DONE, ""); done_tick = i
        if d < 0: d = 0
        max_d = max(max_d, d); max_alt = max(max_alt, alt)
        if st == DONE: break

    ok = viol == 0 and done_tick > 0
    if name == "nominal": ok = ok and first_reason == "MISSION_END" and first_tick == 3000
    if name == "animal": ok = ok and first_reason == "ANIMAL" and 1000 <= first_tick <= 1030 and max_alt >= 45000
    if name == "false_positive": ok = ok and ESCAPE not in visited and first_reason == "MISSION_END"
    if name == "geofence": ok = ok and first_reason == "GEOFENCE" and max_d <= 121000
    if name == "low_battery": ok = ok and first_reason == "BATT_LOW" and first_tick <= 1760
    if name == "gps_loss": ok = ok and first_reason == "GPS_LOSS" and 2090 <= first_tick <= 2110 and RTL_CLIMB not in visited
    if name == "link_loss": ok = ok and first_reason == "LINK_LOSS" and 2140 <= first_tick <= 2160
    if name == "dropout_short": ok = ok and first_reason == "ANIMAL" and 1625 <= first_tick <= 1640 and det_hold > 0
    if name == "dropout_long": ok = ok and first_reason == "DET_STALE" and 1690 <= first_tick <= 1700 and ESCAPE not in visited
    if name == "wind_gust": ok = ok and first_reason == "MISSION_END" and max_d < FENCE - 10000
    print("scenario %d %s" % (sid, name))
    for l in log: print(l)
    print("  first=%s@%d max_d=%d max_alt=%d det_hold=%d gear@%d landed@%d panels@%d done@%d viol=%d"
          % (first_reason, first_tick, max_d, max_alt, det_hold, gear_tick, cover_tick, panel_tick, done_tick, viol))
    print("  %s" % ("PASS" if ok else "FAIL"))
    return ok


n = 0
for s in range(len(SCEN)):
    if run(s): n += 1
print("TOTAL pass=%d/%d" % (n, len(SCEN)))
