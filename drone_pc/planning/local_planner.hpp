#pragma once

#include "common/params.hpp"
#include "common/units.hpp"
#include "planning/global_planner.hpp"

#include <cstdint>
#include <vector>

namespace drone_pc {

struct LocalSetpoint {
    int64_t x_mm = 0;
    int64_t y_mm = 0;
    int64_t z_mm = 0;
    int64_t speed_mm_s = 0;
    bool vlos_limited = false;
    bool altitude_clamped = false;
    bool avoiding = false;
};

inline std::vector<std::vector<int>> line_cells(int ax, int ay, int az, int bx, int by, int bz) {
    const int steps = std::max(std::abs(bx - ax), std::max(std::abs(by - ay), std::abs(bz - az)));
    std::vector<std::vector<int>> cells;
    if (steps == 0) {
        cells.push_back({ax, ay, az});
        return cells;
    }
    for (int index = 0; index <= steps; ++index) {
        const int x = ax + static_cast<int>(div_round(static_cast<int64_t>(bx - ax) * index, steps));
        const int y = ay + static_cast<int>(div_round(static_cast<int64_t>(by - ay) * index, steps));
        const int z = az + static_cast<int>(div_round(static_cast<int64_t>(bz - az) * index, steps));
        if (cells.empty() || cells.back()[0] != x || cells.back()[1] != y || cells.back()[2] != z) {
            cells.push_back({x, y, z});
        }
    }
    return cells;
}

inline int64_t nearest_occupied_mm(const OccupancyGrid& grid, int64_t px, int64_t py, int64_t pz) {
    bool found = false;
    int64_t best_sq = 0;
    int64_t best_dist = 0;
    for (int iz = 0; iz < grid.size()[2]; ++iz) {
        for (int iy = 0; iy < grid.size()[1]; ++iy) {
            for (int ix = 0; ix < grid.size()[0]; ++ix) {
                if (grid.cell(ix, iy, iz) == 0) {
                    continue;
                }
                int64_t cx = 0;
                int64_t cy = 0;
                int64_t cz = 0;
                grid.cell_center(ix, iy, iz, cx, cy, cz);
                const int64_t dx = cx - px;
                const int64_t dy = cy - py;
                const int64_t dz = cz - pz;
                const int64_t dist_sq = dx * dx + dy * dy + dz * dz;
                if (!found || dist_sq < best_sq) {
                    found = true;
                    best_sq = dist_sq;
                    best_dist = hypot_mm(dx, dy, dz);
                }
            }
        }
    }
    return found ? best_dist : -1;
}

inline LocalSetpoint plan_local(
    const OccupancyGrid& grid,
    const std::vector<double>& position_m,
    const std::vector<std::vector<int>>& path,
    const std::vector<double>& home_m,
    const std::string& mode,
    const Limits& limits,
    const std::vector<std::pair<double, double>>& geofence,
    const std::vector<std::vector<std::pair<double, double>>>& no_fly,
    int lookahead_cells,
    double vlos_max_m) {
    const auto fence_mm = polygon_mm(geofence);
    const auto no_fly_mm = polygons_mm(no_fly);
    const int64_t max_alt_mm = scale_milli(limits.alt_max_m);
    const int64_t vlos_mm = scale_milli(vlos_max_m);
    const int64_t px = scale_milli(position_m.at(0));
    const int64_t py = scale_milli(position_m.at(1));
    const int64_t pz = scale_milli(position_m.at(2));
    const int64_t hx = scale_milli(home_m.at(0));
    const int64_t hy = scale_milli(home_m.at(1));
    const int64_t mode_mm = scale_milli(limits.speed_for_mode(mode));
    const int64_t cap_mm = scale_milli(limits.speed_max_mps);
    const int64_t slow_mm = scale_milli(limits.slow_radius_m);
    const int64_t min_z = scale_milli(limits.alt_min_m);
    const int64_t max_z = scale_milli(limits.alt_max_m);
    auto blocked = [&](int ix, int iy, int iz) {
        return cell_blocked(grid, ix, iy, iz, fence_mm, no_fly_mm, true, max_alt_mm);
    };

    bool avoiding = false;
    bool hold = path.empty();
    bool chosen = false;
    int cx = 0;
    int cy = 0;
    int cz = 0;
    if (!path.empty()) {
        int best_i = 0;
        bool have = false;
        int64_t best_d = 0;
        for (size_t index = 0; index < path.size(); ++index) {
            int64_t x = 0;
            int64_t y = 0;
            int64_t z = 0;
            grid.cell_center(path[index][0], path[index][1], path[index][2], x, y, z);
            const int64_t dx = x - px;
            const int64_t dy = y - py;
            const int64_t dz = z - pz;
            const int64_t dist_sq = dx * dx + dy * dy + dz * dz;
            if (!have || dist_sq < best_d) {
                have = true;
                best_d = dist_sq;
                best_i = static_cast<int>(index);
            }
        }
        const int target_i = std::min(best_i + lookahead_cells, static_cast<int>(path.size()) - 1);
        const int tx = path[target_i][0];
        const int ty = path[target_i][1];
        const int tz = path[target_i][2];
        int curx = 0;
        int cury = 0;
        int curz = 0;
        const bool have_current = grid.cell_of_mm(px, py, pz, curx, cury, curz);
        bool blocked_leg = !have_current;
        if (have_current) {
            for (const auto& cell : line_cells(curx, cury, curz, tx, ty, tz)) {
                if (blocked(cell[0], cell[1], cell[2])) {
                    blocked_leg = true;
                    break;
                }
            }
        }
        if (blocked_leg) {
            avoiding = true;
            if (!have_current) {
                hold = true;
            } else {
                int64_t tcx = 0;
                int64_t tcy = 0;
                int64_t tcz = 0;
                grid.cell_center(tx, ty, tz, tcx, tcy, tcz);
                bool have_nb = false;
                int64_t best_nb_d = 0;
                for (const auto& step : kNeighbors) {
                    const int ix = curx + step.dx;
                    const int iy = cury + step.dy;
                    const int iz = curz + step.dz;
                    if (blocked(ix, iy, iz)) {
                        continue;
                    }
                    int64_t nx = 0;
                    int64_t ny = 0;
                    int64_t nz = 0;
                    grid.cell_center(ix, iy, iz, nx, ny, nz);
                    const int64_t ddx = nx - tcx;
                    const int64_t ddy = ny - tcy;
                    const int64_t ddz = nz - tcz;
                    const int64_t dist_sq = ddx * ddx + ddy * ddy + ddz * ddz;
                    if (!have_nb || dist_sq < best_nb_d) {
                        have_nb = true;
                        best_nb_d = dist_sq;
                        cx = ix;
                        cy = iy;
                        cz = iz;
                    }
                }
                if (!have_nb) {
                    hold = true;
                } else {
                    chosen = true;
                }
            }
        } else {
            cx = tx;
            cy = ty;
            cz = tz;
            chosen = true;
        }
    }

    int64_t sx = px;
    int64_t sy = py;
    int64_t sz = pz;
    if (!hold && chosen) {
        grid.cell_center(cx, cy, cz, sx, sy, sz);
    } else if (!path.empty()) {
        avoiding = true;
    }

    bool altitude_clamped = false;
    if (sz < min_z) {
        sz = min_z;
        altitude_clamped = true;
    }
    if (sz > max_z) {
        sz = max_z;
        altitude_clamped = true;
    }
    const int64_t dx = sx - hx;
    const int64_t dy = sy - hy;
    const int64_t dist_sq = dx * dx + dy * dy;
    bool vlos_limited = false;
    if (vlos_mm >= 0 && dist_sq > vlos_mm * vlos_mm) {
        int64_t dist = hypot_mm(dx, dy, 0);
        if (dist <= 0) {
            dist = 1;
        }
        sx = hx + div_round(dx * vlos_mm, dist);
        sy = hy + div_round(dy * vlos_mm, dist);
        vlos_limited = true;
    }
    int64_t speed = 0;
    if (!hold && !path.empty()) {
        const int64_t nearest = nearest_occupied_mm(grid, px, py, pz);
        if (nearest >= 0 && slow_mm > 0 && nearest < slow_mm) {
            speed = div_round(mode_mm * nearest, slow_mm);
        } else {
            speed = mode_mm;
        }
        if (speed > cap_mm) {
            speed = cap_mm;
        }
    }
    LocalSetpoint result;
    result.x_mm = sx;
    result.y_mm = sy;
    result.z_mm = sz;
    result.speed_mm_s = speed;
    result.vlos_limited = vlos_limited;
    result.altitude_clamped = altitude_clamped;
    result.avoiding = avoiding;
    return result;
}

}  // namespace drone_pc
