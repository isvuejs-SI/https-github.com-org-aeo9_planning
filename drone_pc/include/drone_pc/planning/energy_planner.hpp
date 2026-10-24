#pragma once

#include <string>
#include <vector>

#include "drone_pc/params.hpp"
#include "drone_pc/planning/occupancy_map.hpp"

namespace drone_pc {

struct EnergyPlanInput {
  double distance_to_home_km{0.0};
  double soc_percent{100.0};
  double remaining_mission_km{0.0};
  bool overnight{false};
  Point3 current_position;
  std::vector<Point3> candidate_backup_sites;
};

struct EnergyPlanResult {
  double return_reserve_wh{0.0};
  double available_wh{0.0};
  bool return_reserve_ok{false};
  bool stay_overnight{false};
  bool backup_selected{false};
  Point3 backup_site;
  std::string reason;
};

class EnergyPlanner {
 public:
  explicit EnergyPlanner(const Params& params);

  double compute_return_reserve_wh(double distance_km) const;
  EnergyPlanResult evaluate(const EnergyPlanInput& in, const OccupancyMap* map = nullptr) const;

 private:
  Params params_;
};

}  // namespace drone_pc
