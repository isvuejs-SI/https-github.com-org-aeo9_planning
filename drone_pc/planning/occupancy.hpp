#pragma once

#include "common/json.hpp"
#include "common/units.hpp"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace drone_pc {

inline constexpr uint8_t kStatic = 1;
inline constexpr uint8_t kDynamic = 2;

struct Detection {
    std::string kind;
    double x = 0;
    double y = 0;
    double z = 0;
    double vx = 0;
    double vy = 0;
    double vz = 0;
    double radius_m = 0;
};

class OccupancyGrid {
public:
    OccupancyGrid(const std::vector<double>& origin_m, double resolution_m, const std::vector<int>& size, double inflation_m)
        : inflation_m_(inflation_m) {
        origin_ = {scale_milli(origin_m.at(0)), scale_milli(origin_m.at(1)), scale_milli(origin_m.at(2))};
        res_ = scale_milli(resolution_m);
        if (res_ <= 0) {
            throw std::runtime_error("grid resolution must be positive");
        }
        size_ = {size.at(0), size.at(1), size.at(2)};
        cells_.assign(static_cast<size_t>(size_[0] * size_[1] * size_[2]), 0);
    }

    int64_t res() const { return res_; }
    const std::vector<int>& size() const { return size_; }
    const std::vector<uint8_t>& cells() const { return cells_; }
    uint8_t cell(int ix, int iy, int iz) const { return cells_[static_cast<size_t>(index(ix, iy, iz))]; }

    int index(int ix, int iy, int iz) const { return ix + size_[0] * (iy + size_[1] * iz); }

    bool in_bounds(int ix, int iy, int iz) const {
        return ix >= 0 && iy >= 0 && iz >= 0 && ix < size_[0] && iy < size_[1] && iz < size_[2];
    }

    bool cell_of_mm(int64_t x_mm, int64_t y_mm, int64_t z_mm, int& ix, int& iy, int& iz) const {
        ix = static_cast<int>(floor_div(x_mm - origin_[0], res_));
        iy = static_cast<int>(floor_div(y_mm - origin_[1], res_));
        iz = static_cast<int>(floor_div(z_mm - origin_[2], res_));
        return in_bounds(ix, iy, iz);
    }

    bool cell_of_m(double x_m, double y_m, double z_m, int& ix, int& iy, int& iz) const {
        return cell_of_mm(scale_milli(x_m), scale_milli(y_m), scale_milli(z_m), ix, iy, iz);
    }

    void cell_center(int ix, int iy, int iz, int64_t& x, int64_t& y, int64_t& z) const {
        const int64_t half = res_ / 2;
        x = origin_[0] + static_cast<int64_t>(ix) * res_ + half;
        y = origin_[1] + static_cast<int64_t>(iy) * res_ + half;
        z = origin_[2] + static_cast<int64_t>(iz) * res_ + half;
    }

    void mark(int ix, int iy, int iz, uint8_t value) {
        if (!in_bounds(ix, iy, iz)) {
            return;
        }
        uint8_t& current = cells_[static_cast<size_t>(index(ix, iy, iz))];
        if (value == kStatic || current == 0) {
            current = value;
        } else if (value == kDynamic && current != kStatic) {
            current = kDynamic;
        }
    }

    void clear_value(uint8_t value) {
        for (uint8_t& cell : cells_) {
            if (cell == value) {
                cell = 0;
            }
        }
    }

    void mark_ball(int64_t x_mm, int64_t y_mm, int64_t z_mm, int64_t radius_mm, uint8_t value) {
        int ix = 0;
        int iy = 0;
        int iz = 0;
        if (cell_of_mm(x_mm, y_mm, z_mm, ix, iy, iz)) {
            mark(ix, iy, iz, value);
        }
        if (radius_mm <= 0) {
            return;
        }
        int ix0 = static_cast<int>(floor_div(x_mm - radius_mm - origin_[0], res_));
        int iy0 = static_cast<int>(floor_div(y_mm - radius_mm - origin_[1], res_));
        int iz0 = static_cast<int>(floor_div(z_mm - radius_mm - origin_[2], res_));
        int ix1 = static_cast<int>(floor_div(x_mm + radius_mm - origin_[0], res_));
        int iy1 = static_cast<int>(floor_div(y_mm + radius_mm - origin_[1], res_));
        int iz1 = static_cast<int>(floor_div(z_mm + radius_mm - origin_[2], res_));
        if (ix0 < 0) ix0 = 0;
        if (iy0 < 0) iy0 = 0;
        if (iz0 < 0) iz0 = 0;
        if (ix1 > size_[0] - 1) ix1 = size_[0] - 1;
        if (iy1 > size_[1] - 1) iy1 = size_[1] - 1;
        if (iz1 > size_[2] - 1) iz1 = size_[2] - 1;
        const int64_t radius_sq = radius_mm * radius_mm;
        const int64_t half = res_ / 2;
        for (int cz_i = iz0; cz_i <= iz1; ++cz_i) {
            const int64_t cz = origin_[2] + static_cast<int64_t>(cz_i) * res_ + half;
            const int64_t dz = cz - z_mm;
            for (int cy_i = iy0; cy_i <= iy1; ++cy_i) {
                const int64_t cy = origin_[1] + static_cast<int64_t>(cy_i) * res_ + half;
                const int64_t dy = cy - y_mm;
                for (int cx_i = ix0; cx_i <= ix1; ++cx_i) {
                    const int64_t cx = origin_[0] + static_cast<int64_t>(cx_i) * res_ + half;
                    const int64_t dx = cx - x_mm;
                    if (dx * dx + dy * dy + dz * dz <= radius_sq) {
                        mark(cx_i, cy_i, cz_i, value);
                    }
                }
            }
        }
    }

    void insert_points_m(const std::vector<std::vector<double>>& points, uint8_t value = kStatic) {
        const int64_t radius = scale_milli(inflation_m_);
        for (const auto& point : points) {
            mark_ball(scale_milli(point.at(0)), scale_milli(point.at(1)), scale_milli(point.at(2)), radius, value);
        }
    }

    std::vector<std::vector<int>> occupied() const {
        std::vector<std::vector<int>> found;
        for (int iz = 0; iz < size_[2]; ++iz) {
            for (int iy = 0; iy < size_[1]; ++iy) {
                for (int ix = 0; ix < size_[0]; ++ix) {
                    const uint8_t value = cell(ix, iy, iz);
                    if (value) {
                        found.push_back({ix, iy, iz, static_cast<int>(value)});
                    }
                }
            }
        }
        return found;
    }

private:
    std::vector<int64_t> origin_;
    int64_t res_ = 0;
    std::vector<int> size_;
    double inflation_m_ = 0;
    std::vector<uint8_t> cells_;
};

inline void mark_moving(OccupancyGrid& grid, const Detection& detection, double inflation_m, int steps, double dt_s) {
    const int64_t radius_mm = scale_milli(detection.radius_m) + scale_milli(inflation_m);
    const int64_t x0 = scale_milli(detection.x);
    const int64_t y0 = scale_milli(detection.y);
    const int64_t z0 = scale_milli(detection.z);
    const int64_t vx = scale_milli(detection.vx);
    const int64_t vy = scale_milli(detection.vy);
    const int64_t vz = scale_milli(detection.vz);
    const int64_t dt_milli = scale_milli(dt_s);
    for (int step = 0; step <= steps; ++step) {
        const int64_t px = x0 + div_round(vx * dt_milli * step, 1000);
        const int64_t py = y0 + div_round(vy * dt_milli * step, 1000);
        const int64_t pz = z0 + div_round(vz * dt_milli * step, 1000);
        grid.mark_ball(px, py, pz, radius_mm, kDynamic);
    }
}

inline void apply_thermal(OccupancyGrid& grid, const std::vector<Detection>& detections, double inflation_m, int steps, double dt_s) {
    grid.clear_value(kDynamic);
    for (const auto& detection : detections) {
        if (detection.kind != "person" && detection.kind != "animal") {
            continue;
        }
        mark_moving(grid, detection, inflation_m, steps, dt_s);
    }
}

inline std::vector<Detection> parse_thermal_json(const std::string& text) {
    const Json payload = parse_json(text);
    std::vector<Detection> parsed;
    for (const auto& item : payload.at("detections").arr) {
        Detection detection;
        detection.kind = item.at("class").str;
        detection.x = item.at("x").num;
        detection.y = item.at("y").num;
        detection.z = item.at("z").num;
        detection.vx = item.at("vx").num;
        detection.vy = item.at("vy").num;
        detection.vz = item.at("vz").num;
        detection.radius_m = item.at("radius_m").num;
        parsed.push_back(detection);
    }
    return parsed;
}

inline float read_f32_le(const std::vector<uint8_t>& data, size_t offset) {
    uint32_t bits = static_cast<uint32_t>(data.at(offset)) |
                    (static_cast<uint32_t>(data.at(offset + 1)) << 8) |
                    (static_cast<uint32_t>(data.at(offset + 2)) << 16) |
                    (static_cast<uint32_t>(data.at(offset + 3)) << 24);
    float value = 0.0f;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

inline std::vector<std::vector<double>> decode_pointcloud2_xyz(
    const std::vector<uint8_t>& data,
    int point_step,
    int row_step,
    int width,
    int height,
    int x_offset,
    int y_offset,
    int z_offset) {
    std::vector<std::vector<double>> points;
    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            const size_t base = static_cast<size_t>(row * row_step + col * point_step);
            points.push_back({
                static_cast<double>(read_f32_le(data, base + static_cast<size_t>(x_offset))),
                static_cast<double>(read_f32_le(data, base + static_cast<size_t>(y_offset))),
                static_cast<double>(read_f32_le(data, base + static_cast<size_t>(z_offset))),
            });
        }
    }
    return points;
}

}  // namespace drone_pc
