from drone_safety_ops.mission_gate import MissionState, gate, return_wh

ROWS = [
    ("06:30", "PREFLIGHT", "LAUNCH", 3, True, True, True, 90, 400, 8, 25, 0, 0, 4, 22, False,
     "after D2 overnight, SoC 90%"),
    ("07:15", "TRANSIT", "TRANSIT", 3, True, True, True, 78, 400, 5, 25, 0, 2, 5, 28, False,
     "en route to filming site"),
    ("08:40", "FILMING", "FILMING", 3, True, True, True, 70, 400, 6, 25, 0, 1, 5, 31, False,
     "animal: supervisor CLIMB_ESCAPE"),
    ("09:30", "FILMING", "FILMING", 3, True, True, True, 64, 400, 4, 25, 0, 45, 5, 30, False,
     "Starlink/MAVLink gap 45 s"),
    ("09:31", "FILMING", "FILMING", 3, True, True, True, 63, 400, 4, 25, 0, 2, 5, 30, False, "link restored"),
    ("13:30", "RETURN_CHK", "FILMING", 3, True, True, True, 48, 400, 6, 25, 0, 1, 6, 33, False,
     "afternoon reserve check"),
    ("19:00", "OVERNIGHT", "OVERNIGHT", 3, True, True, True, 35, 400, 8, 25, 40, 900, 4, 20, False,
     "cloudy forecast 40 Wh"),
    ("22:10", "OVERNIGHT", "OVERNIGHT", 3, True, True, True, 35, 400, 8, 25, 40, 900, 14, 18, True,
     "gust 14 m/s + intrusion"),
]


def render(params):
    lines = ["AEO-9 DAY 3 (SIM, placeholder numbers; legal area = outside GRCA NP, VLOS + observer)"]
    for row in ROWS:
        t, label, phase, day, legal, observer, remote_id, soc, pack, dist, wh, solar, gap, wind, cell, intrusion, note = row
        state = MissionState(label, phase, day, legal, observer, remote_id, soc, pack, dist, wh, solar, gap, wind, cell,
                             intrusion)
        verdict, why = gate(state, params.mission_gate)
        need = return_wh(state, params.mission_gate)
        lines.append(
            f"{t} {label:<10} SoC{soc:3.0f}% home{dist:3.0f}km gap{gap:4.0f}s wind{wind:3.0f} "
            f"sun{solar:4.0f} need{need:6.1f}Wh -> {verdict:<13} {why} | {note}"
        )
    return "\n".join(lines) + "\n"
