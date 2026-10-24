#include "drone_pc/planning/local_planner.hpp"

#include <algorithm>
#include <cmath>

namespace drone_pc {
namespace {

double dist(const Point3& a, const Point3& b) {
  const double dx = a.x - b.x;
  const double dy = a.y - b.y;
  const double dz = a.z - b.z;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

Point3 clamp_altitude(Point3 p, double min_alt, double max_alt) {
  p.z = std::clamp(p.z, min_alt, max_alt);
  return p;
}

}  // namespace

LocalPlanner::LocalPlanner(const Params& params) : params_(params) {}

LocalPlanOutput LocalPlanner::plan(const OccupancyMap& map, const LocalPlanInput& in) const {
  LocalPlanOutput out;
  Point3 goal = clamp_altitude(in.goal, params_.local_min_altitude_m, params_.local_max_altitude_m);
  Point3 pos = in.position;

  const double range_home = dist(pos, in.home);
  if (range_home > params_.local_vlos_max_range_m) {
    out.setpoint = in.home;
    out.setpoint.z = std::clamp(pos.z, params_.local_min_altitude_m, params_.local_max_altitude_m);
    out.speed_mps = params_.local_min_speed_mps;
    out.stop = false;
    out.reason = "vlos_limit_return_home_direction";
    return out;
  }

  // Nearest occupied distance along path toward goal (sample)
  double nearest_obs = 1e9;
  const double dx = goal.x - pos.x;
  const double dy = goal.y - pos.y;
  const double dz = goal.z - pos.z;
  const double len = std::max(1e-6, std::sqrt(dx * dx + dy * dy + dz * dz));
  for (double t = 0.0; t <= 1.0; t += 0.05) {
    Point3 s{pos.x + dx * t, pos.y + dy * t, pos.z + dz * t};
    if (map.occupied_world(s) || map.in_nofly(s) || !map.inside_geofence(s)) {
      nearest_obs = std::min(nearest_obs, dist(pos, s));
    }
  }

  if (nearest_obs <= params_.local_obstacle_stop_distance_m) {
    // Lateral avoidance step
    Point3 left{pos.x - dy / len * params_.local_avoidance_lateral_step_m,
                pos.y + dx / len * params_.local_avoidance_lateral_step_m, pos.z};
    Point3 right{pos.x + dy / len * params_.local_avoidance_lateral_step_m,
                 pos.y - dx / len * params_.local_avoidance_lateral_step_m, pos.z};
    left = clamp_altitude(left, params_.local_min_altitude_m, params_.local_max_altitude_m);
    right = clamp_altitude(right, params_.local_min_altitude_m, params_.local_max_altitude_m);
    const bool left_ok = !map.occupied_world(left) && map.inside_geofence(left) && !map.in_nofly(left);
    const bool right_ok =
        !map.occupied_world(right) && map.inside_geofence(right) && !map.in_nofly(right);
    if (left_ok) {
      out.setpoint = left;
      out.speed_mps = params_.local_min_speed_mps;
      out.reason = "avoid_left";
      return out;
    }
    if (right_ok) {
      out.setpoint = right;
      out.speed_mps = params_.local_min_speed_mps;
      out.reason = "avoid_right";
      return out;
    }
    out.setpoint = pos;
    out.speed_mps = 0.0;
    out.stop = true;
    out.reason = "obstacle_stop";
    return out;
  }

  double speed = params_.local_max_speed_mps;
  if (nearest_obs <= params_.local_obstacle_slow_distance_m) {
    const double span =
        params_.local_obstacle_slow_distance_m - params_.local_obstacle_stop_distance_m;
    const double t = std::clamp((nearest_obs - params_.local_obstacle_stop_distance_m) / span, 0.0, 1.0);
    speed = params_.local_min_speed_mps + t * (params_.local_max_speed_mps - params_.local_min_speed_mps);
    out.reason = "obstacle_slow";
  } else {
    out.reason = "clear";
  }

  // Step toward goal limited by speed * nominal dt (1s for setpoint target)
  const double step = std::min(len, speed);
  out.setpoint = Point3{pos.x + dx / len * step, pos.y + dy / len * step, pos.z + dz / len * step};
  out.setpoint = clamp_altitude(out.setpoint, params_.local_min_altitude_m, params_.local_max_altitude_m);
  out.speed_mps = speed;
  return out;
}

}  // namespace drone_pc
