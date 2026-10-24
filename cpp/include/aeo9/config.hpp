#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace aeo9 {

struct Waypoint {
  std::string name;
  int32_t north_cm{0};
  int32_t east_cm{0};
  int32_t alt_cm{0};
  std::string leg;
};

struct Config {
  int32_t seed{0};
  bool sim_only{true};
  int32_t max_wind_cm_s{0};
  int32_t reserve_energy_mwh{0};
  int32_t geofence_radius_cm{0};
  int32_t min_altitude_agl_cm{0};
  int32_t capacity_mwh{0};
  int32_t hover_draw_mwh_per_s{0};
  int32_t transit_draw_mwh_per_s{0};
  int32_t film_draw_mwh_per_s{0};
  int32_t solar_charge_mwh_per_s{0};
  int32_t animal_escape_climb_cm{0};
  int32_t animal_escape_stop_after{0};
  int32_t filming_leg_mwh{0};
  int32_t return_leg_mwh{0};
  std::string home_name;
  int32_t home_lat_e7{0};
  int32_t home_lon_e7{0};
  int32_t home_alt_cm{0};
  std::vector<Waypoint> waypoints;
};

Config load_config(const std::string& path);
std::vector<std::string> config_dump_lines(const Config& cfg);

}  // namespace aeo9
