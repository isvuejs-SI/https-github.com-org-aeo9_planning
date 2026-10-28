#pragma once

#include "common/geom.hpp"
#include "common/units.hpp"
#include "planning/global_planner.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace drone_pc {

inline int64_t return_reserve_from_milli_km(
    int64_t distance_milli_km, double wh_per_km, double distance_factor, double pack_wh, double pack_fraction) {
    const int64_t wh_milli = scale_milli(wh_per_km);
    const int64_t factor_milli = scale_milli(distance_factor);
    const int64_t fraction_milli = scale_milli(pack_fraction);
    const int64_t pack_micro = wh_to_micro(pack_wh);
    const int64_t term_distance = div_round(distance_milli_km * wh_milli * factor_milli, 1000);
    const int64_t term_pack = div_round(fraction_milli * pack_micro, 1000);
    return term_distance + term_pack;
}

inline int64_t return_reserve_micro(
    double distance_km, double wh_per_km, double distance_factor, double pack_wh, double pack_fraction) {
    return return_reserve_from_milli_km(scale_milli(distance_km), wh_per_km, distance_factor, pack_wh, pack_fraction);
}

inline int64_t return_reserve_micro_from_mm(
    int64_t distance_mm, double wh_per_km, double distance_factor, double pack_wh, double pack_fraction) {
    return return_reserve_from_milli_km(div_round(distance_mm, 1000), wh_per_km, distance_factor, pack_wh, pack_fraction);
}

inline int64_t available_micro(double soc, double pack_wh) {
    return div_round(scale_milli(soc) * wh_to_micro(pack_wh), 1000);
}

inline std::string overnight_decision(
    double soc,
    double pack_wh,
    double solar_wh,
    double distance_km,
    double mission_wh,
    double wh_per_km,
    double distance_factor,
    double pack_fraction,
    bool airborne) {
    const int64_t available = available_micro(soc, pack_wh);
    const int64_t reserve = return_reserve_micro(distance_km, wh_per_km, distance_factor, pack_wh, pack_fraction);
    const int64_t mission = wh_to_micro(mission_wh);
    const int64_t solar = wh_to_micro(solar_wh);
    const int64_t pack = wh_to_micro(pack_wh);
    const int64_t morning = (available + solar > pack) ? pack : available + solar;
    const int64_t need = mission + reserve;
    if (airborne) {
        if (available < reserve || available < need) {
            return "RETURN_NOW";
        }
        return "CONTINUE";
    }
    if (morning >= need) {
        return "STAY_CHARGE";
    }
    if (available >= reserve) {
        return "RETURN_NOW";
    }
    return "STAY_CHARGE";
}

struct BackupSite {
    int id = 0;
    int64_t x_mm = 0;
    int64_t y_mm = 0;
    int64_t z_mm = 0;
    int64_t distance_mm = 0;
    int64_t reserve_micro_wh = 0;
    bool energy_shortfall = false;
};

struct BackupCandidate {
    int id = 0;
    double x = 0;
    double y = 0;
    double z = 0;
};

inline std::optional<BackupSite> plan_backup(
    const std::vector<double>& origin_m,
    const std::vector<BackupCandidate>& candidates,
    const std::vector<int>& blocked_ids,
    const std::vector<std::pair<double, double>>& geofence,
    const std::vector<std::vector<std::pair<double, double>>>& no_fly,
    double soc,
    double pack_wh,
    double wh_per_km,
    double distance_factor,
    double pack_fraction) {
    const auto fence_mm = polygon_mm(geofence);
    const auto no_fly_mm = polygons_mm(no_fly);
    const int64_t ox = scale_milli(origin_m.at(0));
    const int64_t oy = scale_milli(origin_m.at(1));
    const int64_t available = available_micro(soc, pack_wh);
    struct Record {
        int id;
        int64_t x_mm;
        int64_t y_mm;
        int64_t z_mm;
        int64_t distance_mm;
        int64_t reserve;
        int64_t dist_sq;
    };
    std::vector<Record> legal;
    std::vector<Record> reachable;
    for (const auto& candidate : candidates) {
        bool blocked = false;
        for (int ident : blocked_ids) {
            if (ident == candidate.id) {
                blocked = true;
            }
        }
        if (blocked) {
            continue;
        }
        const int64_t x_mm = scale_milli(candidate.x);
        const int64_t y_mm = scale_milli(candidate.y);
        const int64_t z_mm = scale_milli(candidate.z);
        if (!fence_mm.empty() && !point_in_polygon(x_mm, y_mm, fence_mm)) {
            continue;
        }
        bool in_no_fly = false;
        for (const auto& polygon : no_fly_mm) {
            if (point_in_polygon(x_mm, y_mm, polygon)) {
                in_no_fly = true;
            }
        }
        if (in_no_fly) {
            continue;
        }
        const int64_t dx = x_mm - ox;
        const int64_t dy = y_mm - oy;
        Record record;
        record.id = candidate.id;
        record.x_mm = x_mm;
        record.y_mm = y_mm;
        record.z_mm = z_mm;
        record.dist_sq = dx * dx + dy * dy;
        record.distance_mm = hypot_mm(dx, dy, 0);
        record.reserve = return_reserve_micro_from_mm(record.distance_mm, wh_per_km, distance_factor, pack_wh, pack_fraction);
        legal.push_back(record);
        if (record.reserve <= available) {
            reachable.push_back(record);
        }
    }
    const std::vector<Record>& pool = reachable.empty() ? legal : reachable;
    if (pool.empty()) {
        return std::nullopt;
    }
    Record chosen = pool[0];
    for (const auto& item : pool) {
        if (item.dist_sq < chosen.dist_sq || (item.dist_sq == chosen.dist_sq && item.id < chosen.id)) {
            chosen = item;
        }
    }
    BackupSite site;
    site.id = chosen.id;
    site.x_mm = chosen.x_mm;
    site.y_mm = chosen.y_mm;
    site.z_mm = chosen.z_mm;
    site.distance_mm = chosen.distance_mm;
    site.reserve_micro_wh = chosen.reserve;
    site.energy_shortfall = reachable.empty();
    return site;
}

}  // namespace drone_pc
