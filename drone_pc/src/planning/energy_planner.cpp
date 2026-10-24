#include "drone_pc/planning/energy_planner.hpp"

#include <cmath>
#include <limits>

namespace drone_pc {
namespace {

double dist_xy(const Point3& a, const Point3& b) {
  const double dx = a.x - b.x;
  const double dy = a.y - b.y;
  return std::sqrt(dx * dx + dy * dy);
}

}  // namespace

EnergyPlanner::EnergyPlanner(const Params& params) : params_(params) {}

double EnergyPlanner::compute_return_reserve_wh(double distance_km) const {
  // return reserve = distance × Wh/km × 1.3 + 20% of the pack
  return distance_km * params_.energy_wh_per_km * params_.energy_reserve_distance_factor +
         params_.energy_pack_reserve_fraction * params_.energy_pack_capacity_wh;
}

EnergyPlanResult EnergyPlanner::evaluate(const EnergyPlanInput& in, const OccupancyMap* map) const {
  EnergyPlanResult out;
  out.return_reserve_wh = compute_return_reserve_wh(in.distance_to_home_km);
  out.available_wh = (in.soc_percent / 100.0) * params_.energy_pack_capacity_wh;

  const double mission_wh =
      in.remaining_mission_km * params_.energy_wh_per_km * params_.energy_reserve_distance_factor;
  out.return_reserve_ok = out.available_wh >= (out.return_reserve_wh + mission_wh);

  if (in.overnight) {
    // Overnight solar-versus-return test: stay if solar recovery covers deficit better than flying home
    const double deficit = std::max(0.0, out.return_reserve_wh - out.available_wh);
    out.stay_overnight = params_.energy_overnight_solar_wh_estimate >= deficit;
    out.reason = out.stay_overnight ? "overnight_solar_ok" : "overnight_return_preferred";
  } else {
    out.reason = out.return_reserve_ok ? "energy_ok" : "energy_short";
  }

  double best = std::numeric_limits<double>::infinity();
  for (const auto& site : in.candidate_backup_sites) {
    if (map) {
      if (!map->inside_geofence(site) || map->in_nofly(site) || map->occupied_world(site)) {
        continue;
      }
    }
    const double d = dist_xy(in.current_position, site);
    if (d < params_.energy_min_backup_landing_clearance_m) continue;
    if (d < best) {
      best = d;
      out.backup_site = site;
      out.backup_selected = true;
    }
  }
  if (!out.backup_selected && out.reason == "energy_ok") {
    out.reason = "energy_ok_no_backup";
  } else if (out.backup_selected && out.reason == "energy_ok") {
    out.reason = "energy_ok_backup_selected";
  }
  return out;
}

}  // namespace drone_pc
