# AEO-9 five-day mission supervisor test (integer math, seeded LCG).
# tick = 1 min. Energy in 0.1 Wh (dWh). Distance in m. ALL NUMBERS ARE
# ILLUSTRATIVE PLACEHOLDERS, not real battery, solar, motor or terrain specs.
CAP = 4000            # 400 Wh pack (illustrative)
RESERVE = 800         # launch reserve 20 %
FLOOR = 600           # in-flight hard floor 15 %
CRUISE_W = 100        # dWh per min in transit
FILM_W = 90           # dWh per min loitering
ESC_W = 140           # dWh per min escape climb
IDLE_W = 1            # Jetson + BMS idle on the ground (day); 1 per 4 min asleep at night
UPLINK_W = 4          # Starlink upload, first 60 min after landing
SOLAR_PEAK = 10       # dWh per min at noon, after MPPT (illustrative)
SUNRISE, SUNSET, NOON = 390, 1080, 735
FLY_OPEN, FLY_CLOSE, LAST_LAUNCH = 480, 1020, 840
SPEED = 600           # m per min
WIND_MAX = 11         # m/s launch limit (placeholder)
FENCE_SOFT, FENCE_HARD = 1200, 1500   # corridor half-width, m
ALT = 110             # m above home
CAMPS = [0, 6000, 12000]
# legs: (from camp, to camp, film minutes, optional)
LEGS = [(0, 1, 12, 0), (1, 2, 12, 0), (2, 2, 20, 1), (2, 1, 8, 0), (1, 0, 5, 0)]
SCEN = ["nominal", "cloudy_day1", "wind_day2", "animal_heavy", "fence_drift"]


class Rng:
    def __init__(self, s):
        self.s = s

    def next(self):
        self.s = (self.s * 1103515245 + 12345) & 0x7FFFFFFF
        return self.s >> 8


def terrain(x):   # illustrative relative terrain (m), NOT a DEM
    if x < 5000:
        return 0
    if x < 9000:
        return (x - 5000) * 60 // 4000
    if x < 11000:
        return 60 - (x - 9000) * 60 // 2000
    return 0


def solar(t, cloud):
    if t <= SUNRISE or t >= SUNSET:
        return 0
    return SOLAR_PEAK * (345 - abs(t - NOON)) * cloud // (345 * 100)


def hhmm(t):
    return "%02d:%02d" % (t // 60, t % 60)


def run(sid):
    name = SCEN[sid]
    rng = Rng(2026 + sid)
    soc = CAP; leg = 0; out = []; ok = True
    night_fly = 0; deploy_air = 0; min_soc = CAP; fence_hits = 0; max_y = 0
    min_agl = 9999; max_agl = 0
    for day in range(1, 6):
        cloud = 100 - rng.next() % 15
        wind = 4 + rng.next() % 5
        if name == "cloudy_day1" and day == 1:
            cloud = 35
        if name == "wind_day2" and day == 2:
            wind = 14
        p_animal = 4 if name == "animal_heavy" else 20
        mand_left = sum(1 for L in LEGS[leg:] if not L[3])
        while leg < len(LEGS) and LEGS[leg][3] and mand_left >= 6 - day:
            out.append("  day %d drop optional leg %d (behind schedule)" % (day, leg))
            leg += 1
        state = "GROUND"; launch = -1; land = -1; used = 0; gain = 0
        animals = 0; escapes = 0; x = 0; y = 0; film_left = 0; tgt = 0
        esc_left = 0; reason = ""; soc_launch = 0
        for t in range(1440):
            g = solar(t, cloud) if state == "GROUND" else 0
            gain += g
            soc = min(CAP, soc + g)
            if state == "GROUND":
                if SUNRISE <= t < SUNSET or t % 4 == 0:
                    soc -= IDLE_W
                if land >= 0 and t < land + 60:
                    soc -= UPLINK_W
                if launch < 0 and leg < len(LEGS) and FLY_OPEN <= t <= LAST_LAUNCH:
                    a, b, fm, _ = LEGS[leg]
                    need = (abs(CAMPS[b] - CAMPS[a]) // SPEED + 1) * CRUISE_W + fm * FILM_W + 3 * ESC_W
                    if wind > WIND_MAX:
                        reason = "wind %d m/s" % wind
                    elif soc < need + RESERVE:
                        reason = "charging"
                    else:
                        state = "STOW"; stow = 3; launch = t; reason = ""
                        soc_launch = soc
                        x = CAMPS[a]; y = 0; tgt = CAMPS[b]; film_left = fm
                continue
            if state == "STOW":     # C-Motion stows panels and cover first
                stow -= 1
                soc -= IDLE_W
                if stow == 0:
                    state = "TRANSIT" if x != tgt else "FILM"
                continue
            if t < FLY_OPEN or t > FLY_CLOSE:
                night_fly += 1
            if state == "TRANSIT":
                soc -= CRUISE_W; used += CRUISE_W
                step = min(SPEED, abs(tgt - x))
                x += step if tgt > x else -step
                if name == "fence_drift" and leg == 3:
                    y += 200 + rng.next() % 200
                if abs(y) > FENCE_SOFT:
                    y -= 400 if y > 0 else -400
                if abs(y) > FENCE_HARD:
                    fence_hits += 1
                max_y = max(max_y, abs(y))
                if x == tgt:
                    state = "FILM"
            elif state == "FILM":
                if esc_left > 0:
                    soc -= ESC_W; used += ESC_W; esc_left -= 1
                else:
                    soc -= FILM_W; used += FILM_W; film_left -= 1
                    if rng.next() % p_animal == 0:
                        animals += 1
                        if rng.next() % 3 == 0:   # close + confident: escape
                            escapes += 1; esc_left = 2
                dist_home = abs(tgt - x)
                if film_left <= 0 or soc < FLOOR + RESERVE // 2 + dist_home // SPEED * CRUISE_W:
                    state = "LAND"
            agl = ALT - terrain(x)
            min_agl = min(min_agl, agl); max_agl = max(max_agl, agl)
            if state == "LAND":
                land = t; state = "GROUND"; leg += 1
                deploy_air += 0          # deploy only issued in GROUND state
            min_soc = min(min_soc, soc)
        if launch < 0:
            out.append("  day %d SCRUB (%s) soc_end=%d.%d%% solar=%dWh" %
                       (day, reason or "no leg", soc * 100 // CAP, soc * 1000 // CAP % 10, gain // 10))
        else:
            out.append("  day %d leg %d launch %s land %s used=%dWh solar=%dWh soc launch=%d%% day_end=%d%% animals=%d escapes=%d" %
                       (day, leg - 1, hhmm(launch), hhmm(land), used // 10, gain // 10,
                        soc_launch * 100 // CAP, soc * 100 // CAP, animals, escapes))
    home = leg == len(LEGS)
    ok = home and night_fly == 0 and deploy_air == 0 and min_soc >= FLOOR and fence_hits == 0 and min_agl >= 40
    out.append("  RESULT %s home=%d night_fly=%d min_soc=%d%% fence=%d max_y=%dm agl=%d..%dm %s" %
               (name, 1 if home else 0, night_fly, min_soc * 100 // CAP, fence_hits, max_y, min_agl, max_agl,
                "PASS" if ok else "FAIL"))
    return out, ok


print("AEO-9 5-day mission sim (illustrative numbers, seed 2026+id)")
n = 0
for i in range(len(SCEN)):
    print("scenario %d %s" % (i, SCEN[i]))
    lines, ok = run(i)
    for L in lines:
        print(L)
    n += ok
print("TOTAL pass=%d/%d" % (n, len(SCEN)))
