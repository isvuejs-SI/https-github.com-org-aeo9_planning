#pragma once

#include "common/geom.hpp"
#include "common/units.hpp"
#include "planning/occupancy.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <queue>
#include <utility>
#include <vector>

namespace drone_pc {

struct CellDelta {
    int dx;
    int dy;
    int dz;
};

inline const CellDelta kNeighbors[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};

inline std::vector<std::pair<int64_t, int64_t>> polygon_mm(const std::vector<std::pair<double, double>>& polygon) {
    std::vector<std::pair<int64_t, int64_t>> converted;
    for (const auto& point : polygon) {
        converted.emplace_back(scale_milli(point.first), scale_milli(point.second));
    }
    return converted;
}

inline std::vector<std::vector<std::pair<int64_t, int64_t>>> polygons_mm(
    const std::vector<std::vector<std::pair<double, double>>>& polygons) {
    std::vector<std::vector<std::pair<int64_t, int64_t>>> converted;
    for (const auto& polygon : polygons) {
        converted.push_back(polygon_mm(polygon));
    }
    return converted;
}

inline bool cell_blocked(
    const OccupancyGrid& grid,
    int ix,
    int iy,
    int iz,
    const std::vector<std::pair<int64_t, int64_t>>& fence_mm,
    const std::vector<std::vector<std::pair<int64_t, int64_t>>>& no_fly_mm,
    bool has_max_alt,
    int64_t max_alt_mm) {
    if (!grid.in_bounds(ix, iy, iz)) {
        return true;
    }
    if (grid.cell(ix, iy, iz) != 0) {
        return true;
    }
    int64_t cx = 0;
    int64_t cy = 0;
    int64_t cz = 0;
    grid.cell_center(ix, iy, iz, cx, cy, cz);
    if (has_max_alt && cz > max_alt_mm) {
        return true;
    }
    if (!fence_mm.empty() && !point_in_polygon(cx, cy, fence_mm)) {
        return true;
    }
    for (const auto& polygon : no_fly_mm) {
        if (point_in_polygon(cx, cy, polygon)) {
            return true;
        }
    }
    return false;
}

inline std::vector<std::vector<int>> plan_path(
    const OccupancyGrid& grid,
    const std::vector<int>& start,
    const std::vector<int>& goal,
    const std::vector<std::pair<double, double>>& geofence,
    const std::vector<std::vector<std::pair<double, double>>>& no_fly,
    double max_alt_m) {
    const auto fence_mm = polygon_mm(geofence);
    const auto no_fly_mm = polygons_mm(no_fly);
    const int64_t max_alt_mm = scale_milli(max_alt_m);
    const int sx = start.at(0);
    const int sy = start.at(1);
    const int sz = start.at(2);
    const int gx = goal.at(0);
    const int gy = goal.at(1);
    const int gz = goal.at(2);
    auto blocked = [&](int ix, int iy, int iz) {
        return cell_blocked(grid, ix, iy, iz, fence_mm, no_fly_mm, true, max_alt_mm);
    };
    if (blocked(sx, sy, sz) || blocked(gx, gy, gz)) {
        return {};
    }
    const int nx = grid.size()[0];
    const int ny = grid.size()[1];
    const int nz = grid.size()[2];
    const int count = nx * ny * nz;
    std::vector<int64_t> gscore(static_cast<size_t>(count), -1);
    std::vector<int> parent(static_cast<size_t>(count), -1);
    auto heuristic = [&](int ix, int iy, int iz) -> int64_t {
        return (std::abs(ix - gx) + std::abs(iy - gy) + std::abs(iz - gz)) * grid.res();
    };
    struct Node {
        int64_t f;
        int64_t h;
        int iz;
        int iy;
        int ix;
        bool operator>(const Node& other) const {
            if (f != other.f) return f > other.f;
            if (h != other.h) return h > other.h;
            if (iz != other.iz) return iz > other.iz;
            if (iy != other.iy) return iy > other.iy;
            return ix > other.ix;
        }
    };
    std::priority_queue<Node, std::vector<Node>, std::greater<Node>> open;
    const int start_index = grid.index(sx, sy, sz);
    const int64_t h0 = heuristic(sx, sy, sz);
    gscore[static_cast<size_t>(start_index)] = 0;
    open.push(Node{h0, h0, sz, sy, sx});
    bool found = false;
    while (!open.empty()) {
        const Node node = open.top();
        open.pop();
        const int current = grid.index(node.ix, node.iy, node.iz);
        const int64_t g_cost = gscore[static_cast<size_t>(current)];
        if (g_cost < 0 || g_cost + node.h != node.f) {
            continue;
        }
        if (node.ix == gx && node.iy == gy && node.iz == gz) {
            found = true;
            break;
        }
        for (const auto& step : kNeighbors) {
            const int ix = node.ix + step.dx;
            const int iy = node.iy + step.dy;
            const int iz = node.iz + step.dz;
            if (blocked(ix, iy, iz)) {
                continue;
            }
            const int64_t next_g = g_cost + grid.res();
            const int neighbor = grid.index(ix, iy, iz);
            const int64_t previous = gscore[static_cast<size_t>(neighbor)];
            if (previous < 0 || next_g < previous) {
                gscore[static_cast<size_t>(neighbor)] = next_g;
                parent[static_cast<size_t>(neighbor)] = current;
                const int64_t next_h = heuristic(ix, iy, iz);
                open.push(Node{next_g + next_h, next_h, iz, iy, ix});
            }
        }
    }
    if (!found) {
        return {};
    }
    std::vector<std::vector<int>> path;
    int cursor = grid.index(gx, gy, gz);
    while (true) {
        const int ix = cursor % nx;
        const int rest = cursor / nx;
        const int iy = rest % ny;
        const int iz = rest / ny;
        path.push_back({ix, iy, iz});
        if (cursor == start_index) {
            break;
        }
        cursor = parent[static_cast<size_t>(cursor)];
    }
    std::reverse(path.begin(), path.end());
    return path;
}

}  // namespace drone_pc
