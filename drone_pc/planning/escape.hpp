#pragma once

#include "common/params.hpp"
#include "common/units.hpp"
#include "planning/occupancy.hpp"

#include <optional>
#include <string>
#include <vector>

namespace drone_pc {

inline int escape_rank(const std::string& name) {
    if (name == "STAY") return 0;
    if (name == "WATCH") return 1;
    if (name == "DETER") return 2;
    if (name == "ESCAPE") return 3;
    if (name == "PROP_LOCKOUT") return 4;
    return -1;
}

inline std::string verdict_for_distance_mm(int64_t distance_mm, const Limits& limits) {
    if (distance_mm <= scale_milli(limits.escape_prop_lockout_range_m)) return "PROP_LOCKOUT";
    if (distance_mm <= scale_milli(limits.escape_escape_range_m)) return "ESCAPE";
    if (distance_mm <= scale_milli(limits.escape_deter_range_m)) return "DETER";
    if (distance_mm <= scale_milli(limits.escape_watch_range_m)) return "WATCH";
    return "STAY";
}

inline std::string worst_escape(const std::vector<std::string>& names) {
    if (names.empty()) {
        return "STAY";
    }
    std::string best = names[0];
    for (const auto& name : names) {
        if (escape_rank(name) > escape_rank(best)) {
            best = name;
        }
    }
    return best;
}

struct EscapeSetpoint {
    int64_t x_mm = 0;
    int64_t y_mm = 0;
    int64_t z_mm = 0;
};

struct EscapeResult {
    std::string verdict;
    std::string action;
    std::optional<EscapeSetpoint> setpoint;
    std::vector<std::string> requests;
    std::optional<std::string> mission_event;
};

inline std::vector<std::string> requests_for_escape(const std::string& action) {
    if (action == "CLIMB_TO_BACKUP") {
        return {"ARM", "TAKEOFF", "OFFBOARD_SETPOINT"};
    }
    if (action == "HOLD_AND_ALERT") {
        return {"HOLD"};
    }
    return {};
}

inline EscapeResult evaluate_escape(
    const std::vector<double>& drone_m,
    const std::vector<Detection>& detections,
    double soc,
    bool cover_open,
    double wind_mps,
    const Limits& limits,
    const std::optional<std::vector<double>>& backup_m) {
    std::vector<std::string> verdicts;
    for (const auto& detection : detections) {
        if (detection.kind != "person" && detection.kind != "animal") {
            continue;
        }
        const int64_t dx = scale_milli(detection.x - drone_m.at(0));
        const int64_t dy = scale_milli(detection.y - drone_m.at(1));
        const int64_t dz = scale_milli(detection.z - drone_m.at(2));
        verdicts.push_back(verdict_for_distance_mm(hypot_mm(dx, dy, dz), limits));
    }
    EscapeResult result;
    result.verdict = worst_escape(verdicts);
    if (result.verdict == "PROP_LOCKOUT") {
        result.action = "DO_NOT_ARM";
    } else if (result.verdict == "ESCAPE") {
        const bool soc_ok = scale_milli(soc) >= scale_milli(limits.escape_min_soc);
        const bool wind_ok = scale_milli(wind_mps) <= scale_milli(limits.escape_max_wind_mps);
        if (soc_ok && cover_open && wind_ok && backup_m.has_value()) {
            result.action = "CLIMB_TO_BACKUP";
        } else {
            result.action = "HOLD_AND_ALERT";
        }
    } else if (result.verdict == "DETER") {
        result.action = "ALERT_DETER";
    } else if (result.verdict == "WATCH") {
        result.action = "ALERT_WATCH";
    } else {
        result.action = "NONE";
    }
    if (result.action == "CLIMB_TO_BACKUP") {
        EscapeSetpoint setpoint;
        setpoint.x_mm = scale_milli(backup_m->at(0));
        setpoint.y_mm = scale_milli(backup_m->at(1));
        setpoint.z_mm = scale_milli(drone_m.at(2)) + scale_milli(limits.escape_climb_m);
        result.setpoint = setpoint;
        result.mission_event = "ESCAPE_TRIGGER";
    }
    result.requests = requests_for_escape(result.action);
    return result;
}

}  // namespace drone_pc
