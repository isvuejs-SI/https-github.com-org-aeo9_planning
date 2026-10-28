#pragma once

#include <cmath>
#include <cstdint>

namespace drone_pc {

inline int64_t round_half_away(double value) {
    if (value >= 0.0) {
        return static_cast<int64_t>(std::floor(value + 0.5));
    }
    return static_cast<int64_t>(std::ceil(value - 0.5));
}

inline int64_t scale_milli(double value) { return round_half_away(value * 1000.0); }

inline int64_t wh_to_micro(double value) { return round_half_away(value * 1000000.0); }

inline int64_t floor_div(int64_t numer, int64_t denom) {
    if (denom < 0) {
        numer = -numer;
        denom = -denom;
    }
    int64_t quot = numer / denom;
    int64_t rem = numer % denom;
    if (rem != 0 && numer < 0) {
        --quot;
    }
    return quot;
}

inline int64_t div_round(int64_t numer, int64_t denom) {
    if (denom < 0) {
        numer = -numer;
        denom = -denom;
    }
    if (numer >= 0) {
        return (numer + denom / 2) / denom;
    }
    return -((-numer + denom / 2) / denom);
}

inline int64_t hypot_mm(int64_t dx, int64_t dy, int64_t dz = 0) {
    const double sum = static_cast<double>(dx * dx + dy * dy + dz * dz);
    return round_half_away(std::sqrt(sum));
}

}  // namespace drone_pc
