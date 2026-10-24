#pragma once

#include <string>

#include "drone_pc/params.hpp"
#include "drone_pc/planning/occupancy_map.hpp"

namespace drone_pc {

struct LocalPlanInput {
  Point3 position;
  Point3 goal;
  Point3 home;  // for VLOS range from operator/home
  double current_speed_mps{0.0};
};

struct LocalPlanOutput {
  Point3 setpoint;
  double speed_mps{0.0};
  bool stop{false};
  std::string reason;
};

class LocalPlanner {
 public:
  explicit LocalPlanner(const Params& params);

  LocalPlanOutput plan(const OccupancyMap& map, const LocalPlanInput& in) const;

 private:
  Params params_;
};

}  // namespace drone_pc
