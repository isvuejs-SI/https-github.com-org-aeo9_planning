#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "drone_pc/params.hpp"

namespace drone_pc {

struct Point3 {
  double x{0.0};
  double y{0.0};
  double z{0.0};
};

struct MovingObstacle {
  Point3 position;
  std::string label;  // person | animal | other
  double radius_m{1.0};
};

struct Polygon2 {
  std::vector<Point3> vertices;  // z ignored; XY polygon
};

class OccupancyMap {
 public:
  explicit OccupancyMap(const Params& params);

  int size_x() const { return size_x_; }
  int size_y() const { return size_y_; }
  int size_z() const { return size_z_; }
  double resolution() const { return resolution_; }

  void clear();
  void integrate_points(const std::vector<Point3>& points);
  void add_moving_obstacles(const std::vector<MovingObstacle>& obstacles);
  void set_nofly_polygons(const std::vector<Polygon2>& polygons);
  void set_geofence(const Polygon2& fence);

  bool world_to_index(const Point3& p, int& ix, int& iy, int& iz) const;
  Point3 index_to_world(int ix, int iy, int iz) const;

  bool occupied(int ix, int iy, int iz) const;
  bool occupied_world(const Point3& p) const;
  bool inside_geofence(const Point3& p) const;
  bool in_nofly(const Point3& p) const;

  // Hard constraint for planners: free, in geofence, not in no-fly
  bool is_traversable(int ix, int iy, int iz) const;

 private:
  std::size_t index(int ix, int iy, int iz) const;
  static bool point_in_polygon(double x, double y, const Polygon2& poly);

  double resolution_;
  int size_x_, size_y_, size_z_;
  double origin_x_, origin_y_, origin_z_;
  double occupied_threshold_;
  double inflate_m_;
  std::vector<float> occupancy_;
  std::vector<Polygon2> nofly_;
  Polygon2 geofence_;
  bool geofence_set_{false};
};

}  // namespace drone_pc
