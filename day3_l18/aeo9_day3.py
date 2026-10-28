# AEO-9 Day 3 (film-only at camp 2) supervisor replay. 1-min ticks, integer math.
# ALL numbers are illustrative placeholders, not AEO-9 or vendor specs. Units: dWh (0.1 Wh), dm/s, m.
import sys
CAP, RESERVE, FLOOR = 4000, 800, 600
CRUISE, FILM, ESC, IDLE, SLEEP, UPL = 100, 70, 140, 2, 1, 4
SUNRISE, SUNSET, NOON, PEAK = 390, 1080, 735, 10
WIND_MAX, GUST_ABORT = 110, 140
OPEN, LAST = 480, 840
FILM_MIN, MAX_ESC, CAM_FAILS = 15, 3, 3
UP_NEED, UP_DEADLINE = 60, 1200
ALT, MIN_AGL = 75, 40
D4_NEED = 13 * CRUISE + 8 * FILM + 3 * ESC   # camp2->camp1 leg + film, placeholder

class LCG:
    def __init__(s, seed): s.x = seed
    def next(s, m):
        s.x = (s.x * 1103515245 + 12345) % 2147483648
        return (s.x >> 8) % m

def solar(t, cloud):
    t %= 1440
    if t <= SUNRISE or t >= SUNSET: return 0
    return PEAK * (345 - abs(t - NOON)) * cloud // (345 * 100)

def terrain(x):  # local relief around camp 2, m above camp (placeholder ridge)
    return 30 - abs((x % 800) - 400) // 10 if x % 800 else 30

SC = [  # name, dawn_soc, wind, gust_at, det_rate, outage(start,len), cloud_am, cloud_pm, cam_fail_at, behind
    ("nominal",        3200, 60, -1, 6, (-1, 0), 100, 100, -1, 0),
    ("gust_filming",   3200, 60, 12, 6, (-1, 0), 100, 100, -1, 0),
    ("repeat_escapes", 3200, 60, -1, 40, (-1, 0), 100, 100, -1, 0),
    ("starlink_outage",3200, 60, -1, 6, (0, 150), 100, 100, -1, 0),
    ("cloudy_afternoon",3200, 60, -1, 6, (-1, 0), 100, 25, -1, 0),
    ("camera_trigger_fail",3200, 60, -1, 6, (-1, 0), 100, 100, 7, 0),
    ("behind_schedule",3200, 60, -1, 6, (-1, 0), 100, 100, -1, 1),
]

def hm(t): return "D%d %02d:%02d" % (3 + t // 1440, (t % 1440) // 60, t % 60)

def run(i, sc, out, csv):
    name, soc, wind, gust_at, det, outage, cam, cpm, camfail, behind = sc
    r = LCG(2026 + i)
    need = 5 * CRUISE + FILM_MIN * FILM + 2 * ESC + 5 * CRUISE
    st, t0, film, esc, dets, fails, trig, x = "NIGHT", 0, 0, 0, 0, 0, 0, 0
    up, landed_at, minsoc, minagl, ev = 0, -1, soc, 999, []
    out.append("# %s dawn_soc=%d need=%d+%d" % (name, soc, need, RESERVE))
    d4go = -1
    for t in range(0, 1440 + LAST + 1):
        cloud = cam if (t % 1440) < 840 or t >= 1440 else cpm
        draw, gain = 0, solar(t, cloud) if st in ("NIGHT", "WAIT", "DEPLOYED", "UPLOAD", "CHARGE", "DONE") else 0
        if st == "NIGHT":
            draw = SLEEP
            if t >= OPEN and t < 1440: st = "WAIT"
        elif st == "WAIT":
            draw = IDLE
            if behind: ev.append("%s SKIP day3 (behind schedule)" % hm(t)); st = "CHARGE"
            elif wind > WIND_MAX: pass
            elif soc >= need + RESERVE: ev.append("%s GO soc=%d" % (hm(t), soc)); st, t0 = "STOW", t
            elif t >= LAST: ev.append("%s NO-GO energy" % hm(t)); st = "CHARGE"
        elif st == "STOW":
            draw = IDLE
            if t - t0 >= 3: st, t0 = "CLIMB", t
        elif st == "CLIMB":
            draw = CRUISE
            if t - t0 >= 5: st, t0 = "FILM", t
        elif st == "FILM":
            draw, film, x = FILM, film + 1, x + 120
            ok = not (camfail >= 0 and film >= camfail)
            trig += 1
            fails = 0 if ok else fails + 1
            g = wind + (90 if gust_at >= 0 and film == gust_at else r.next(20))
            agl = ALT - terrain(x)
            if r.next(100) < det:
                dets += 1
                if r.next(100) < 50:
                    esc += 1; draw = ESC; agl += 35
                    ev.append("%s ESCAPE #%d climb +35 m" % (hm(t), esc))
            minagl = min(minagl, agl)
            reason = ""
            if g > GUST_ABORT: reason = "gust %d dm/s" % g
            elif esc >= MAX_ESC: reason = "escape limit"
            elif fails >= CAM_FAILS: reason = "camera trigger fail x%d" % fails
            elif soc < FLOOR + 5 * CRUISE: reason = "energy floor"
            elif film >= FILM_MIN: reason = "film done"
            if reason: ev.append("%s RTL camp2 (%s) film=%d min" % (hm(t), reason, film)); st, t0 = "RETURN", t
        elif st == "RETURN":
            draw = CRUISE
            if t - t0 >= 5: st, t0, landed_at = "DEPLOY", t, t; ev.append("%s LANDED camp2" % hm(t))
        elif st == "DEPLOY":
            draw = IDLE
            if t - t0 >= 3: st, t0 = "UPLOAD", t
        elif st == "UPLOAD":
            link = not (outage[0] >= 0 and t - t0 < outage[1])
            draw = IDLE + (UPL if link else 0)
            up += 1 if link else 0
            if up >= UP_NEED: ev.append("%s UPLOAD done" % hm(t)); st = "CHARGE"
            elif t % 1440 >= UP_DEADLINE: ev.append("%s UPLOAD deferred to D4 (%d/%d min)" % (hm(t), up, UP_NEED)); st = "CHARGE"
        elif st == "CHARGE":
            draw = IDLE
            if t % 1440 >= SUNSET: st = "DONE"
        elif st == "DONE":
            draw = SLEEP
        soc = max(0, min(CAP, soc - draw + gain))
        minsoc = min(minsoc, soc)
        csv.append("%s,%d,%s,%d,%d,%d" % (name, t, st, draw, gain, soc))
        if t == 1439: ev.append("D3 23:59 soc=%d" % soc)
        if t == 1440 + OPEN: soc8 = soc
        if t >= 1440 + OPEN and d4go < 0 and soc >= D4_NEED + RESERVE: d4go = t
    ready = d4go >= 0
    out.extend("  " + e for e in ev)
    out.append("  dets=%d esc=%d triggers=%d min_soc=%d min_agl=%s D4_08:00 soc=%d need=%d -> %s" % (
        dets, esc, trig, minsoc, minagl if minagl < 999 else "-", soc8, D4_NEED + RESERVE, ("D4_GO " + hm(d4go)) if ready else "D4_HOLD_CHARGE"))
    exp = name != "cloudy_afternoon"   # expected: cloudy PM means D4 holds at camp 2
    out.append("  %s" % ("PASS" if ready == exp and minsoc >= FLOOR and (minagl == 999 or minagl >= MIN_AGL) else "FAIL"))
    return ready == exp and minsoc >= FLOOR and (minagl == 999 or minagl >= MIN_AGL)

out, csv, ok = [], ["scenario,minute,state,draw_dwh,solar_dwh,soc_dwh"], 0
for i, sc in enumerate(SC): ok += run(i, sc, out, csv)
out.append("TOTAL pass=%d/%d" % (ok, len(SC)))
print("\n".join(out))
open(sys.argv[1] if len(sys.argv) > 1 else "budget_py.csv", "w").write("\n".join(csv) + "\n")
