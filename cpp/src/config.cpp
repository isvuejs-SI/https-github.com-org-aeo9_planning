#include "aeo9/config.hpp"

#include <stdexcept>
#include <yaml-cpp/yaml.h>

namespace aeo9 {
namespace {

int32_t require_int(const YAML::Node& node, const char* key) {
  if (!node[key]) {
    throw std::runtime_error(std::string("missing config key: ") + key);
  }
  return node[key].as<int32_t>();
}

}  // namespace

Config load_config(const std::string& path) {
  YAML::Node raw = YAML::LoadFile(path);
  Config cfg;
  cfg.seed = require_int(raw, "seed");
  cfg.sim_only = raw["sim_only"].as<bool>();

  const YAML::Node limits = raw["limits"];
  cfg.max_wind_cm_s = require_int(limits, "max_wind_cm_s");
  cfg.reserve_energy_mwh = require_int(limits, "reserve_energy_mwh");
  cfg.geofence_radius_cm = require_int(limits, "geofence_radius_cm");
  cfg.min_altitude_agl_cm = require_int(limits, "min_altitude_agl_cm");

  const YAML::Node energy = raw["energy"];
  cfg.capacity_mwh = require_int(energy, "capacity_mwh");
  cfg.hover_draw_mwh_per_s = require_int(energy, "hover_draw_mwh_per_s");
  cfg.transit_draw_mwh_per_s = require_int(energy, "transit_draw_mwh_per_s");
  cfg.film_draw_mwh_per_s = require_int(energy, "film_draw_mwh_per_s");
  cfg.solar_charge_mwh_per_s = require_int(energy, "solar_charge_mwh_per_s");

  const YAML::Node mission = raw["mission"];
  cfg.animal_escape_climb_cm = require_int(mission, "animal_escape_climb_cm");
  cfg.animal_escape_stop_after = require_int(mission, "animal_escape_stop_after");
  cfg.filming_leg_mwh = require_int(mission, "filming_leg_mwh");
  cfg.return_leg_mwh = require_int(mission, "return_leg_mwh");

  const YAML::Node home = raw["home"];
  cfg.home_name = home["name"].as<std::string>();
  cfg.home_lat_e7 = require_int(home, "lat_e7");
  cfg.home_lon_e7 = require_int(home, "lon_e7");
  cfg.home_alt_cm = require_int(home, "alt_cm");

  if (raw["waypoints"]) {
    for (const auto& item : raw["waypoints"]) {
      Waypoint wp;
      wp.name = item["name"].as<std::string>();
      wp.north_cm = item["north_cm"].as<int32_t>();
      wp.east_cm = item["east_cm"].as<int32_t>();
      wp.alt_cm = item["alt_cm"].as<int32_t>();
      wp.leg = item["leg"].as<std::string>();
      cfg.waypoints.push_back(wp);
    }
  }
  return cfg;
}

std::vector<std::string> config_dump_lines(const Config& cfg) {
  return {
      "seed=" + std::to_string(cfg.seed),
      "sim_only=" + std::to_string(cfg.sim_only ? 1 : 0),
      "max_wind_cm_s=" + std::to_string(cfg.max_wind_cm_s),
      "reserve_energy_mwh=" + std::to_string(cfg.reserve_energy_mwh),
      "geofence_radius_cm=" + std::to_string(cfg.geofence_radius_cm),
      "min_altitude_agl_cm=" + std::to_string(cfg.min_altitude_agl_cm),
      "capacity_mwh=" + std::to_string(cfg.capacity_mwh),
      "home=" + cfg.home_name,
      "home_lat_e7=" + std::to_string(cfg.home_lat_e7),
      "home_lon_e7=" + std::to_string(cfg.home_lon_e7),
      "waypoints=" + std::to_string(cfg.waypoints.size()),
  };
}

}  // namespace aeo9
