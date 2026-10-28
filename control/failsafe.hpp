#pragma once
// Supervisor failsafe. Latch never steps back toward NOMINAL.
// PX4 keeps its own failsafes. This state only chooses the next request.
#include <stdexcept>
#include <string>

inline int fs_rank(const std::string& name) {
    if (name == "NOMINAL") return 0;
    if (name == "HOLD") return 1;
    if (name == "RTL") return 2;
    if (name == "LAND") return 3;
    throw std::runtime_error(name);
}

inline std::string fs_request(const std::string& state, const std::string& req) {
    if (fs_rank(req) > fs_rank(state)) return req;
    return state;
}
