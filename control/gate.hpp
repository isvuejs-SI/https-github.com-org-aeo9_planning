#pragma once
// Go/no-go in fixed order: wind, battery, schedule, fence.
#include <string>

struct Decision {
    std::string action;
    std::string why;
};

inline Decision go_nogo(int wind, int wind_max, int energy, int need, int reserve,
                        int behind, long long dist2, int radius) {
    Decision d;
    if (wind > wind_max) {
        d.action = "WAIT";
        d.why = "wind";
        return d;
    }
    if (energy < need + reserve) {
        d.action = "WAIT";
        d.why = "battery";
        return d;
    }
    if (behind) {
        d.action = "SKIP";
        d.why = "schedule";
        return d;
    }
    long long fence2 = (long long)radius * (long long)radius;
    if (dist2 > fence2) {
        d.action = "SKIP";
        d.why = "fence";
        return d;
    }
    d.action = "GO";
    d.why = "ok";
    return d;
}
