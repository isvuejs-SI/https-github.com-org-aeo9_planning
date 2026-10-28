#pragma once

#include <cstdint>
#include <utility>
#include <vector>

namespace drone_pc {

inline bool point_in_polygon(int64_t x, int64_t y, const std::vector<std::pair<int64_t, int64_t>>& polygon) {
    const int count = static_cast<int>(polygon.size());
    if (count < 3) {
        return false;
    }
    bool inside = false;
    for (int index = 0; index < count; ++index) {
        const int64_t x1 = polygon[index].first;
        const int64_t y1 = polygon[index].second;
        const auto& next = polygon[(index + 1) % count];
        const int64_t x2 = next.first;
        const int64_t y2 = next.second;
        if ((y1 > y) != (y2 > y)) {
            const int64_t lhs = (x - x1) * (y2 - y1);
            const int64_t rhs = (x2 - x1) * (y - y1);
            const bool hit = (y2 - y1) > 0 ? lhs < rhs : lhs > rhs;
            if (hit) {
                inside = !inside;
            }
        }
    }
    return inside;
}

}  // namespace drone_pc
