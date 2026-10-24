#pragma once

#include <string>

namespace drone_pc {

struct Params {
  // Preflight
  double preflight_max_wind_mps{12.0};
  double preflight_max_cell_temp_c{45.0};
  double preflight_min_cell_temp_c{5.0};
  double preflight_min_soc_percent{35.0};
  bool preflight_require_remote_id{true};
  bool preflight_require_geofence_ok{true};
  bool preflight_require_return_reserve{true};

  // Energy
  double energy_wh_per_km{80.0};
  double energy_reserve_distance_factor{1.3};
  double energy_pack_reserve_fraction{0.20};
  double energy_pack_capacity_wh{400.0};
  double energy_escape_min_soc_percent{30.0};
  double energy_overnight_solar_wh_estimate{120.0};
  double energy_min_backup_landing_clearance_m{15.0};

  // Map
  double map_resolution_m{1.0};
  int map_size_x{100};
  int map_size_y{100};
  int map_size_z{40};
  double map_origin_x_m{-50.0};
  double map_origin_y_m{-50.0};
  double map_origin_z_m{0.0};
  double map_occupied_threshold{0.65};
  double map_moving_obstacle_inflate_m{2.0};

  // Global planner
  double global_heuristic_weight{1.0};
  int global_max_expansions{200000};

  // Local planner
  double local_max_speed_mps{8.0};
  double local_min_speed_mps{0.5};
  double local_max_altitude_m{120.0};
  double local_min_altitude_m{5.0};
  double local_obstacle_stop_distance_m{4.0};
  double local_obstacle_slow_distance_m{12.0};
  double local_vlos_max_range_m{500.0};
  double local_avoidance_lateral_step_m{3.0};

  // Escape
  double escape_climb_altitude_m{25.0};
  double escape_watch_radius_m{30.0};
  double escape_deter_radius_m{15.0};
  double escape_escape_radius_m{8.0};
  double escape_prop_lockout_radius_m{3.0};

  // Control
  double control_offboard_rate_hz{20.0};
  double control_heartbeat_period_s{0.05};
  int control_heartbeat_miss_limit{3};
  double control_default_takeoff_altitude_m{10.0};

  // Link loss
  double link_loss_rtl_after_s{30.0};
  double link_loss_abort_after_s{600.0};

  // Alerts / logging
  std::string alerts_ntfy_base_url{"https://ntfy.sh"};
  std::string alerts_ntfy_topic{"drone_pc_alerts"};
  double alerts_duplicate_holdback_s{60.0};
  bool alerts_enabled{true};
  std::string logging_log_directory{"/tmp/drone_pc_logs"};
  bool logging_log_verdicts{true};
  bool logging_log_transitions{true};

  static Params defaults();
};

}  // namespace drone_pc
