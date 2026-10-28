"""Leg energy and the day plan: launch, transit, filming, return, overnight."""

from planner.config_loader import get_int


class Leg(object):
    def __init__(self, name, dist, film, optional):
        self.name = name
        self.dist = dist
        self.film = film
        self.optional = optional


def leg_need(dist, film_min, speed, cruise, film_w, esc_w, nesc):
    transit = abs(dist) // speed + 1
    return transit * cruise + film_min * film_w + nesc * esc_w


def sortie_need(dist, film_min, speed, cruise, film_w, esc_w, nesc):
    """Out and back, plus filming and an escape budget."""
    one = abs(dist) // speed + 1
    return one * cruise * 2 + film_min * film_w + nesc * esc_w


def legs_from_cfg(cfg):
    camp = get_int(cfg, "route.camp_m")
    home = get_int(cfg, "route.home_m")
    dist = camp - home
    if dist < 0:
        dist = -dist
    return [
        Leg("out", dist, get_int(cfg, "planner.out_film_min"), 0),
        Leg("film", 0, get_int(cfg, "planner.hover_film_min"), 1),
        Leg("home", dist, get_int(cfg, "planner.home_film_min"), 0),
    ]


def day_plan(legs, drop_optional):
    phases = ["launch"]
    for leg in legs:
        if drop_optional and leg.optional:
            continue
        phases.append("transit")
        phases.append("filming")
        phases.append("return")
    phases.append("overnight")
    return phases
