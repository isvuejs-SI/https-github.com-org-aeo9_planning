#pragma once

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace drone_safety {

struct Labeled {
  double value{0};
  std::string origin;
};

struct Params {
  struct Supervisor {
    double batt_no_takeoff_pct{30};
    double cell_temp_limit_c{60};
    double batt_rtl_pct{25};
    double batt_land_pct{10};
    double gear_down_alt_m{5};
    double deploy_wind_max_ms{10};
    double detector_timeout_s{2};
  } supervisor;

  struct Mission {
    double reserve_fraction{0.20};
    double energy_margin{1.3};
    double link_rtl_s{30};
    double link_abort_s{600};
    int max_days{5};
    double overnight_wind_hold_ms{10};
    double flight_wind_rtl_ms{12};
    double cell_overtemp_c{60};
    double wh_per_km{25};
    double default_pack_wh{400};
  } mission_gate;

  struct Guard {
    double human_lockout_m{5};
    double watch_beyond_m{15};
    double animal_escape_within_m{8};
    double animal_closing_mps{0.3};
    double escape_min_soc_pct{30};
  } guard;

  struct Landing {
    Labeled leg_km{44.5, "relayed"};
    Labeled batt_endurance_min{14.0, "relayed"};
    Labeled cruise_kmh{40.0, "assumed"};
    double reserve_fraction{0.20};
    double energy_margin{1.3};
    double cell_hold_c{45};
    double soc_hold_fraction{0.95};
  } landing;

  struct Route {
    double straight_line_mi{68};
    double mi_to_km{1.609344};
    Labeled leg_km{44.5, "relayed"};
    Labeled batt_endurance_min{14.0, "relayed"};
    Labeled cruise_kmh{40.0, "assumed"};
    double reserve_fraction{0.20};
    double energy_margin{1.3};
    bool real_flight{false};
    bool tribal_permission{false};
    bool faa_waiver{false};
  } route;

  struct Alert {
    double duplicate_holdback_s{60};
    std::string ntfy_host{"ntfy.sh"};
    std::string default_topic{"isaac-drone-CHANGE-ME"};
  } alert;

  struct Watchdog {
    double period_s{1};
    double systemd_watchdog_sec{5};
  } watchdog;

  struct Logging {
    std::string nvme_dir{"/mnt/nvme/drone_safety_ops"};
    std::string fallback_dir{"/tmp/drone_safety_ops_logs"};
  } logging;

  struct Px4 {
    std::string transport{"uxrce_dds"};
    std::string uxrce_command_topic{"/fmu/in/vehicle_command"};
    std::string request_topic{"/drone_safety_ops/px4_vehicle_command"};
    std::string no_arm_latch_topic{"/drone_safety_ops/no_arm_latch"};
  } px4;
};

inline std::string trim_copy(std::string s) {
  auto not_space = [](unsigned char c) { return !std::isspace(c); };
  s.erase(s.begin(), std::find_if(s.begin(), s.end(), not_space));
  s.erase(std::find_if(s.rbegin(), s.rend(), not_space).base(), s.end());
  return s;
}

inline std::map<std::string, std::string> parse_yaml_subset(const std::string& path) {
  std::ifstream in(path);
  if (!in) throw std::runtime_error("cannot open params: " + path);
  std::map<std::string, std::string> out;
  std::vector<std::pair<int, std::string>> stack;
  std::string line;
  while (std::getline(in, line)) {
    auto hash = line.find('#');
    if (hash != std::string::npos) line = line.substr(0, hash);
    if (trim_copy(line).empty()) continue;
    int indent = 0;
    while (indent < static_cast<int>(line.size()) && line[indent] == ' ') ++indent;
    auto body = trim_copy(line);
    auto colon = body.find(':');
    if (colon == std::string::npos) throw std::runtime_error("bad yaml line: " + body);
    std::string key = trim_copy(body.substr(0, colon));
    std::string rest = trim_copy(body.substr(colon + 1));
    while (!stack.empty() && stack.back().first >= indent) stack.pop_back();
    if (rest.empty()) {
      stack.push_back({indent, key});
      continue;
    }
    std::string path_key;
    for (const auto& frame : stack) {
      if (!path_key.empty()) path_key += ".";
      path_key += frame.second;
    }
    if (!path_key.empty()) path_key += ".";
    path_key += key;
    out[path_key] = rest;
  }
  return out;
}

inline double yaml_double(const std::map<std::string, std::string>& y, const std::string& key) {
  auto it = y.find(key);
  if (it == y.end()) throw std::runtime_error("missing param " + key);
  return std::stod(it->second);
}

inline int yaml_int(const std::map<std::string, std::string>& y, const std::string& key) {
  return static_cast<int>(yaml_double(y, key));
}

inline bool yaml_bool(const std::map<std::string, std::string>& y, const std::string& key) {
  auto it = y.find(key);
  if (it == y.end()) throw std::runtime_error("missing param " + key);
  if (it->second == "true") return true;
  if (it->second == "false") return false;
  throw std::runtime_error("bad bool " + key);
}

inline std::string yaml_string(const std::map<std::string, std::string>& y, const std::string& key) {
  auto it = y.find(key);
  if (it == y.end()) throw std::runtime_error("missing param " + key);
  return it->second;
}

inline Labeled yaml_labeled(const std::map<std::string, std::string>& y, const std::string& key) {
  return Labeled{yaml_double(y, key + ".value"), yaml_string(y, key + ".origin")};
}

inline Params load_params(const std::string& path) {
  auto y = parse_yaml_subset(path);
  Params p;
  p.supervisor.batt_no_takeoff_pct = yaml_double(y, "supervisor.batt_no_takeoff_pct");
  p.supervisor.cell_temp_limit_c = yaml_double(y, "supervisor.cell_temp_limit_c");
  p.supervisor.batt_rtl_pct = yaml_double(y, "supervisor.batt_rtl_pct");
  p.supervisor.batt_land_pct = yaml_double(y, "supervisor.batt_land_pct");
  p.supervisor.gear_down_alt_m = yaml_double(y, "supervisor.gear_down_alt_m");
  p.supervisor.deploy_wind_max_ms = yaml_double(y, "supervisor.deploy_wind_max_ms");
  p.supervisor.detector_timeout_s = yaml_double(y, "supervisor.detector_timeout_s");

  p.mission_gate.reserve_fraction = yaml_double(y, "mission_gate.reserve_fraction");
  p.mission_gate.energy_margin = yaml_double(y, "mission_gate.energy_margin");
  p.mission_gate.link_rtl_s = yaml_double(y, "mission_gate.link_rtl_s");
  p.mission_gate.link_abort_s = yaml_double(y, "mission_gate.link_abort_s");
  p.mission_gate.max_days = yaml_int(y, "mission_gate.max_days");
  p.mission_gate.overnight_wind_hold_ms = yaml_double(y, "mission_gate.overnight_wind_hold_ms");
  p.mission_gate.flight_wind_rtl_ms = yaml_double(y, "mission_gate.flight_wind_rtl_ms");
  p.mission_gate.cell_overtemp_c = yaml_double(y, "mission_gate.cell_overtemp_c");
  p.mission_gate.wh_per_km = yaml_double(y, "mission_gate.wh_per_km");
  p.mission_gate.default_pack_wh = yaml_double(y, "mission_gate.default_pack_wh");

  p.guard.human_lockout_m = yaml_double(y, "guard.human_lockout_m");
  p.guard.watch_beyond_m = yaml_double(y, "guard.watch_beyond_m");
  p.guard.animal_escape_within_m = yaml_double(y, "guard.animal_escape_within_m");
  p.guard.animal_closing_mps = yaml_double(y, "guard.animal_closing_mps");
  p.guard.escape_min_soc_pct = yaml_double(y, "guard.escape_min_soc_pct");

  p.landing.leg_km = yaml_labeled(y, "landing.leg_km");
  p.landing.batt_endurance_min = yaml_labeled(y, "landing.batt_endurance_min");
  p.landing.cruise_kmh = yaml_labeled(y, "landing.cruise_kmh");
  p.landing.reserve_fraction = yaml_double(y, "landing.reserve_fraction");
  p.landing.energy_margin = yaml_double(y, "landing.energy_margin");
  p.landing.cell_hold_c = yaml_double(y, "landing.cell_hold_c");
  p.landing.soc_hold_fraction = yaml_double(y, "landing.soc_hold_fraction");

  p.route.straight_line_mi = yaml_double(y, "route.straight_line_mi");
  p.route.mi_to_km = yaml_double(y, "route.mi_to_km");
  p.route.leg_km = yaml_labeled(y, "route.leg_km");
  p.route.batt_endurance_min = yaml_labeled(y, "route.batt_endurance_min");
  p.route.cruise_kmh = yaml_labeled(y, "route.cruise_kmh");
  p.route.reserve_fraction = yaml_double(y, "route.reserve_fraction");
  p.route.energy_margin = yaml_double(y, "route.energy_margin");
  p.route.real_flight = yaml_bool(y, "route.real_flight");
  p.route.tribal_permission = yaml_bool(y, "route.tribal_permission");
  p.route.faa_waiver = yaml_bool(y, "route.faa_waiver");

  p.alert.duplicate_holdback_s = yaml_double(y, "alert.duplicate_holdback_s");
  p.alert.ntfy_host = yaml_string(y, "alert.ntfy_host");
  p.alert.default_topic = yaml_string(y, "alert.default_topic");

  p.watchdog.period_s = yaml_double(y, "watchdog.period_s");
  p.watchdog.systemd_watchdog_sec = yaml_double(y, "watchdog.systemd_watchdog_sec");

  p.logging.nvme_dir = yaml_string(y, "logging.nvme_dir");
  p.logging.fallback_dir = yaml_string(y, "logging.fallback_dir");

  p.px4.transport = yaml_string(y, "px4.transport");
  p.px4.uxrce_command_topic = yaml_string(y, "px4.uxrce_command_topic");
  p.px4.request_topic = yaml_string(y, "px4.request_topic");
  p.px4.no_arm_latch_topic = yaml_string(y, "px4.no_arm_latch_topic");
  return p;
}

}  // namespace drone_safety
