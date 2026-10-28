"""Close confident detection climbs climb_m. Filming stops after max_esc."""


def escape_check(filming, close, conf, conf_min, escapes, max_esc, climb_m):
    if not filming:
        return escapes, 0, 0, "not_filming"
    if close and conf >= conf_min:
        escapes += 1
        stop = 1 if escapes >= max_esc else 0
        return escapes, climb_m, stop, "climb"
    return escapes, 0, 0, "ignore"
