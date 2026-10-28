#pragma once
// Leg energy and the day plan: launch, transit, filming, return, overnight.
#include "planner/config_loader.hpp"
#include <string>
#include <vector>

struct Leg {
    std::string name;
    int dist;
    int film;
    int optional;
};

inline int iabs_i(int v) { return v < 0 ? -v : v; }

inline int leg_need(int dist, int film_min, int speed, int cruise, int film_w, int esc_w, int nesc) {
    int transit = iabs_i(dist) / speed + 1;
    return transit * cruise + film_min * film_w + nesc * esc_w;
}

inline int sortie_need(int dist, int film_min, int speed, int cruise, int film_w, int esc_w, int nesc) {
    int one = iabs_i(dist) / speed + 1;
    return one * cruise * 2 + film_min * film_w + nesc * esc_w;
}

inline std::vector<Leg> legs_from_cfg(const Tree& cfg) {
    int dist = get_int(cfg, "route.camp_m") - get_int(cfg, "route.home_m");
    if (dist < 0) dist = -dist;
    std::vector<Leg> legs;
    legs.push_back(Leg{"out", dist, get_int(cfg, "planner.out_film_min"), 0});
    legs.push_back(Leg{"film", 0, get_int(cfg, "planner.hover_film_min"), 1});
    legs.push_back(Leg{"home", dist, get_int(cfg, "planner.home_film_min"), 0});
    return legs;
}

inline std::vector<std::string> day_plan(const std::vector<Leg>& legs, int drop_optional) {
    std::vector<std::string> phases;
    phases.push_back("launch");
    for (std::size_t i = 0; i < legs.size(); ++i) {
        if (drop_optional && legs[i].optional) continue;
        phases.push_back("transit");
        phases.push_back("filming");
        phases.push_back("return");
    }
    phases.push_back("overnight");
    return phases;
}

inline std::string join_comma(const std::vector<std::string>& parts) {
    std::string s;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i) s += ",";
        s += parts[i];
    }
    return s;
}
