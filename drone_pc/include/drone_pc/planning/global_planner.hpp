#pragma once

#include <vector>

#include "drone_pc/params.hpp"
#include "drone_pc/planning/occupancy_map.hpp"

namespace drone_pc {

struct GlobalPlanResult {
  bool success{false};
  std::vector<Point3> path;
  int expansions{0};
  std::string reason;
};

class GlobalPlanner {
 public:
  explicit GlobalPlanner(const Params& params);

  GlobalPlanResult plan(const OccupancyMap& map, const Point3& start, const Point3& goal) const;

 private:
  Params params_;
};

}  // namespace drone_pc
