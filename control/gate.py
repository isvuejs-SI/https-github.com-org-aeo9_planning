"""Go/no-go in fixed order: wind, battery, schedule, fence."""


def go_nogo(wind, wind_max, energy, need, reserve, behind, dist2, radius):
    if wind > wind_max:
        return "WAIT", "wind"
    if energy < need + reserve:
        return "WAIT", "battery"
    if behind:
        return "SKIP", "schedule"
    if dist2 > radius * radius:
        return "SKIP", "fence"
    return "GO", "ok"
