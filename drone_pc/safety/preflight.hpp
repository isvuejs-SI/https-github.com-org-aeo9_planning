#pragma once

#include "common/geom.hpp"
#include "common/params.hpp"
#include "common/units.hpp"
#include "planning/energy.hpp"
#include "planning/global_planner.hpp"

#include <map>
#include <string>
#include <vector>

namespace drone_pc {

inline int verdict_rank(const std::string& name) {
    if (name == "GO") return 0;
    if (name == "HOLD_GROUND") return 1;
    if (name == "RTL") return 2;
    if (name == "ABORT_MISSION") return 3;
    if (name == "LAND_NOW") return 4;
    if (name == "NO_GO") return 5;
    throw std::runtime_error("unknown verdict");
}

inline std::string worst_verdict(const std::vector<std::string>& names) {
    if (names.empty()) {
        throw std::runtime_error("no verdicts");
    }
    std::string worst = names[0];
    for (const auto& name : names) {
        if (verdict_rank(name) > verdict_rank(worst)) {
            worst = name;
        }
    }
    return worst;
}

struct PreflightResult {
    std::string verdict;
    std::string reason;
    std::map<std::string, std::string> checks;
};

inline PreflightResult evaluate_preflight(
    const Limits& limits,
    double wind_mps,
    double cell_temp_c,
    double soc,
    double x_m,
    double y_m,
    double alt_m,
    const std::string& remote_id,
    double distance_km,
    double mission_wh,
    bool operator_abort) {
    std::map<std::string, std::string> checks;
    checks["abort_switch"] = operator_abort ? "ABORT_MISSION" : "GO";

    const int64_t temp = scale_milli(cell_temp_c);
    std::string temperature = "GO";
    if (temp <= scale_milli(limits.temp_no_go_low_c) || temp >= scale_milli(limits.temp_no_go_high_c)) {
        temperature = "NO_GO";
    } else if (temp <= scale_milli(limits.temp_land_now_low_c) || temp >= scale_milli(limits.temp_land_now_high_c)) {
        temperature = "LAND_NOW";
    } else if (temp < scale_milli(limits.temp_hold_low_c) || temp > scale_milli(limits.temp_hold_high_c)) {
        temperature = "HOLD_GROUND";
    }
    checks["cell_temp"] = temperature;

    const int64_t x_mm = scale_milli(x_m);
    const int64_t y_mm = scale_milli(y_m);
    const int64_t alt = scale_milli(alt_m);
    const auto fence = polygon_mm(limits.geofence);
    const auto no_fly = polygons_mm(limits.no_fly);
    std::string geofence = "GO";
    if (!fence.empty() && !point_in_polygon(x_mm, y_mm, fence)) {
        geofence = "NO_GO";
    } else {
        for (const auto& polygon : no_fly) {
            if (point_in_polygon(x_mm, y_mm, polygon)) {
                geofence = "NO_GO";
            }
        }
        if (geofence == "GO" && alt > scale_milli(limits.alt_max_m)) {
            geofence = "NO_GO";
        } else if (geofence == "GO" && alt < scale_milli(limits.alt_min_m)) {
            geofence = "HOLD_GROUND";
        }
    }
    checks["geofence"] = geofence;

    if (remote_id == "ok") {
        checks["remote_id"] = "GO";
    } else if (remote_id == "degraded") {
        checks["remote_id"] = "HOLD_GROUND";
    } else {
        checks["remote_id"] = "NO_GO";
    }

    const int64_t available = available_micro(soc, limits.pack_capacity_wh);
    const int64_t reserve = return_reserve_micro(
        distance_km, limits.wh_per_km, limits.return_distance_factor, limits.pack_capacity_wh, limits.pack_reserve_fraction);
    const int64_t mission = wh_to_micro(mission_wh);
    if (available >= reserve + mission) {
        checks["return_reserve"] = "GO";
    } else if (available >= reserve) {
        checks["return_reserve"] = "HOLD_GROUND";
    } else {
        checks["return_reserve"] = "NO_GO";
    }

    const int64_t level = scale_milli(soc);
    if (level >= scale_milli(limits.soc_go_min)) {
        checks["soc"] = "GO";
    } else if (level >= scale_milli(limits.soc_hold_min)) {
        checks["soc"] = "HOLD_GROUND";
    } else if (level >= scale_milli(limits.soc_rtl_min)) {
        checks["soc"] = "RTL";
    } else {
        checks["soc"] = "NO_GO";
    }

    const int64_t wind = scale_milli(wind_mps);
    if (wind <= scale_milli(limits.wind_go_max_mps)) {
        checks["wind"] = "GO";
    } else if (wind <= scale_milli(limits.wind_hold_max_mps)) {
        checks["wind"] = "HOLD_GROUND";
    } else {
        checks["wind"] = "NO_GO";
    }

    std::vector<std::string> values;
    for (const auto& item : checks) {
        values.push_back(item.second);
    }
    PreflightResult result;
    result.checks = checks;
    result.verdict = worst_verdict(values);
    if (result.verdict == "GO") {
        result.reason = "all_clear";
    } else {
        bool first = true;
        for (const auto& item : checks) {
            if (item.second == result.verdict) {
                if (!first) {
                    result.reason += "+";
                }
                result.reason += item.first;
                first = false;
            }
        }
    }
    return result;
}

}  // namespace drone_pc
