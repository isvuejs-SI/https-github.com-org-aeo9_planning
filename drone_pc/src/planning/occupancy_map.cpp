#include "drone_pc/planning/occupancy_map.hpp"

#include <cmath>

namespace drone_pc {

OccupancyMap::OccupancyMap(const Params& params)
    : resolution_(params.map_resolution_m),
      size_x_(params.map_size_x),
      size_y_(params.map_size_y),
      size_z_(params.map_size_z),
      origin_x_(params.map_origin_x_m),
      origin_y_(params.map_origin_y_m),
      origin_z_(params.map_origin_z_m),
      occupied_threshold_(params.map_occupied_threshold),
      inflate_m_(params.map_moving_obstacle_inflate_m),
      occupancy_(static_cast<std::size_t>(size_x_ * size_y_ * size_z_), 0.0f) {}

std::size_t OccupancyMap::index(int ix, int iy, int iz) const {
  return static_cast<std::size_t>((iz * size_y_ + iy) * size_x_ + ix);
}

void OccupancyMap::clear() {
  std::fill(occupancy_.begin(), occupancy_.end(), 0.0f);
  nofly_.clear();
  geofence_set_ = false;
  geofence_ = Polygon2{};
}

bool OccupancyMap::world_to_index(const Point3& p, int& ix, int& iy, int& iz) const {
  ix = static_cast<int>(std::floor((p.x - origin_x_) / resolution_));
  iy = static_cast<int>(std::floor((p.y - origin_y_) / resolution_));
  iz = static_cast<int>(std::floor((p.z - origin_z_) / resolution_));
  return ix >= 0 && iy >= 0 && iz >= 0 && ix < size_x_ && iy < size_y_ && iz < size_z_;
}

Point3 OccupancyMap::index_to_world(int ix, int iy, int iz) const {
  return Point3{
      origin_x_ + (ix + 0.5) * resolution_,
      origin_y_ + (iy + 0.5) * resolution_,
      origin_z_ + (iz + 0.5) * resolution_,
  };
}

void OccupancyMap::integrate_points(const std::vector<Point3>& points) {
  for (const auto& p : points) {
    int ix, iy, iz;
    if (!world_to_index(p, ix, iy, iz)) continue;
    auto& cell = occupancy_[index(ix, iy, iz)];
    cell = std::min(1.0f, cell + 0.35f);
  }
}

void OccupancyMap::add_moving_obstacles(const std::vector<MovingObstacle>& obstacles) {
  const int inflate = static_cast<int>(std::ceil((inflate_m_) / resolution_));
  for (const auto& obs : obstacles) {
    int cx, cy, cz;
    if (!world_to_index(obs.position, cx, cy, cz)) continue;
    const int r = std::max(inflate, static_cast<int>(std::ceil(obs.radius_m / resolution_)));
    for (int dz = -r; dz <= r; ++dz) {
      for (int dy = -r; dy <= r; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
          if (dx * dx + dy * dy + dz * dz > r * r) continue;
          const int ix = cx + dx;
          const int iy = cy + dy;
          const int iz = cz + dz;
          if (ix < 0 || iy < 0 || iz < 0 || ix >= size_x_ || iy >= size_y_ || iz >= size_z_) {
            continue;
          }
          occupancy_[index(ix, iy, iz)] = 1.0f;
        }
      }
    }
  }
}

void OccupancyMap::set_nofly_polygons(const std::vector<Polygon2>& polygons) { nofly_ = polygons; }

void OccupancyMap::set_geofence(const Polygon2& fence) {
  geofence_ = fence;
  geofence_set_ = !fence.vertices.empty();
}

bool OccupancyMap::point_in_polygon(double x, double y, const Polygon2& poly) {
  bool inside = false;
  const auto& v = poly.vertices;
  const std::size_t n = v.size();
  if (n < 3) return false;
  for (std::size_t i = 0, j = n - 1; i < n; j = i++) {
    const double xi = v[i].x, yi = v[i].y;
    const double xj = v[j].x, yj = v[j].y;
    const bool intersect =
        ((yi > y) != (yj > y)) && (x < (xj - xi) * (y - yi) / ((yj - yi) + 1e-12) + xi);
    if (intersect) inside = !inside;
  }
  return inside;
}

bool OccupancyMap::occupied(int ix, int iy, int iz) const {
  if (ix < 0 || iy < 0 || iz < 0 || ix >= size_x_ || iy >= size_y_ || iz >= size_z_) {
    return true;
  }
  return occupancy_[index(ix, iy, iz)] >= static_cast<float>(occupied_threshold_);
}

bool OccupancyMap::occupied_world(const Point3& p) const {
  int ix, iy, iz;
  if (!world_to_index(p, ix, iy, iz)) return true;
  return occupied(ix, iy, iz);
}

bool OccupancyMap::inside_geofence(const Point3& p) const {
  if (!geofence_set_) return true;
  return point_in_polygon(p.x, p.y, geofence_);
}

bool OccupancyMap::in_nofly(const Point3& p) const {
  for (const auto& poly : nofly_) {
    if (point_in_polygon(p.x, p.y, poly)) return true;
  }
  return false;
}

bool OccupancyMap::is_traversable(int ix, int iy, int iz) const {
  if (occupied(ix, iy, iz)) return false;
  const Point3 w = index_to_world(ix, iy, iz);
  if (!inside_geofence(w)) return false;
  if (in_nofly(w)) return false;
  return true;
}

}  // namespace drone_pc
