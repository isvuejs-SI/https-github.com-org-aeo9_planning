#pragma once
// Close confident detection climbs climb_m. Filming stops after max_esc.
#include <string>

struct Esc {
    int escapes;
    int climb;
    int stop;
    std::string why;
};

inline Esc escape_check(int filming, int close, int conf, int conf_min,
                        int escapes, int max_esc, int climb_m) {
    Esc e;
    e.escapes = escapes;
    e.climb = 0;
    e.stop = 0;
    if (!filming) {
        e.why = "not_filming";
        return e;
    }
    if (close && conf >= conf_min) {
        e.escapes = escapes + 1;
        e.climb = climb_m;
        e.stop = e.escapes >= max_esc ? 1 : 0;
        e.why = "climb";
        return e;
    }
    e.why = "ignore";
    return e;
}
