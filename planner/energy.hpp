#pragma once
// E_next = min(C, max(0, E - draw + solar)). Launch when E >= E_need + E_reserve.

inline int energy_next(int energy, int cap, int draw, int solar) {
    int val = energy - draw + solar;
    if (val < 0) val = 0;
    if (val > cap) val = cap;
    return val;
}

inline int launch_gate(int energy, int need, int reserve) {
    return energy >= need + reserve ? 1 : 0;
}

inline int solar_at(int t, int peak, int sunrise, int sunset, int noon, int cloud) {
    if (t <= sunrise || t >= sunset) return 0;
    int half = (sunset - sunrise) / 2;
    int diff = t - noon;
    if (diff < 0) diff = -diff;
    return peak * (half - diff) * cloud / (half * 100);
}
