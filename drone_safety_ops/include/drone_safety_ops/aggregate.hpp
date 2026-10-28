#pragma once

#include "drone_safety_ops/guard.hpp"
#include "drone_safety_ops/json_mini.hpp"
#include "drone_safety_ops/landing.hpp"
#include "drone_safety_ops/mission_gate.hpp"
#include "drone_safety_ops/px4_request.hpp"
#include "drone_safety_ops/route.hpp"
#include "drone_safety_ops/supervisor.hpp"

#include <string>
#include <utility>
#include <vector>

namespace drone_safety {

inline int verdict_severity(const std::string& v) {
  if (v == "PROP_LOCKOUT") return 100;
  if (v == "NO_GO") return 90;
  if (v == "NO_TAKEOFF") return 80;
  if (v == "LAND_NOW") return 72;
  if (v == "LAND") return 70;
  if (v == "ABORT_MISSION") return 60;
  if (v == "RTL") return 50;
  if (v == "CLIMB_ESCAPE") return 40;
  if (v == "ESCAPE") return 35;
  if (v == "HOLD") return 30;
  if (v == "HOLD_GROUND") return 28;
  if (v == "DETER") return 20;
  if (v == "WATCH") return 10;
  return 0;
}

struct Vote {
  std::string module;
  std::string verdict;
  bool counted{true};
};

inline std::pair<std::string, std::string> worst_vote(const std::vector<Vote>& votes) {
  std::string module;
  std::string verdict;
  int best = -1;
  for (const auto& vote : votes) {
    if (!vote.counted) continue;
    int rank = verdict_severity(vote.verdict);
    if (rank > best) {
      best = rank;
      module = vote.module;
      verdict = vote.verdict;
    }
  }
  return {module, verdict};
}

struct Cache {
  double soc_pct{0};
  double cell_temp_c{25};
  double pack_wh{0};
  bool airborne{false};
  bool armed{false};
  bool cover_open{false};
  double wind_ms{0};
  bool rain{false};
  bool wind_ok{false};
  bool gps_ok{false};
  bool inside_fence{false};
  double alt_m{0};
  double dist_home_km{0};
  bool legal_area_ok{false};
  bool geofence_ok{false};
  bool remote_id_ok{false};
  bool observer_ok{false};
  std::string phase{"LAUNCH"};
  int day{1};
  double solar_forecast_wh{0};
  double link_gap_s{0};
  bool detector_ok{false};
  std::string lidar_kind{"none"};
  double lidar_dist{0};
  double lidar_closing{0};
  std::string thermal_kind{"none"};
  double thermal_dist{0};
  double thermal_closing{0};
  bool props_ok{false};
  bool intrusion{false};
  double wh_per_km{0};
  double leg_km{0};
};

inline void apply_present(const Json& j, const char* key, bool& dest) {
  if (j.has(key)) dest = j.boolean(key);
}
inline void apply_present(const Json& j, const char* key, double& dest) {
  if (j.has(key) && j.types.at(key) == "number") dest = j.num(key);
}
inline void apply_present_str(const Json& j, const char* key, std::string& dest) {
  if (j.has(key)) dest = j.str(key);
}

inline void apply_battery(Cache& c, const Json& j) {
  apply_present(j, "soc_pct", c.soc_pct);
  apply_present(j, "cell_temp_c", c.cell_temp_c);
  apply_present(j, "pack_wh", c.pack_wh);
  apply_present(j, "airborne", c.airborne);
  apply_present(j, "armed", c.armed);
  apply_present(j, "cover_open", c.cover_open);
  apply_present(j, "wind_ms", c.wind_ms);
  apply_present(j, "rain", c.rain);
  apply_present(j, "wind_ok", c.wind_ok);
  apply_present(j, "props_ok", c.props_ok);
}

inline void apply_gps(Cache& c, const Json& j) {
  apply_present(j, "gps_ok", c.gps_ok);
  apply_present(j, "inside_fence", c.inside_fence);
  apply_present(j, "alt_m", c.alt_m);
  apply_present(j, "dist_home_km", c.dist_home_km);
  apply_present(j, "legal_area_ok", c.legal_area_ok);
  apply_present(j, "geofence_ok", c.geofence_ok);
}

inline void apply_link(Cache& c, const Json& j) {
  apply_present(j, "remote_id_ok", c.remote_id_ok);
  apply_present(j, "observer_ok", c.observer_ok);
  apply_present_str(j, "phase", c.phase);
  if (j.has("day")) c.day = static_cast<int>(j.num("day"));
  apply_present(j, "solar_forecast_wh", c.solar_forecast_wh);
  apply_present(j, "link_gap_s", c.link_gap_s);
  apply_present(j, "intrusion", c.intrusion);
}

inline void apply_lidar(Cache& c, const Json& j) {
  apply_present_str(j, "kind", c.lidar_kind);
  apply_present(j, "dist_m", c.lidar_dist);
  apply_present(j, "closing_mps", c.lidar_closing);
  apply_present(j, "intrusion", c.intrusion);
}

inline void apply_thermal(Cache& c, const Json& j) {
  apply_present_str(j, "kind", c.thermal_kind);
  apply_present(j, "dist_m", c.thermal_dist);
  apply_present(j, "closing_mps", c.thermal_closing);
  apply_present(j, "cell_temp_c", c.cell_temp_c);
  apply_present(j, "intrusion", c.intrusion);
  if (j.has("kind") || j.has("dist_m")) c.detector_ok = true;
}

struct Snapshot {
  std::string supervisor;
  std::string deploy;
  std::string gate;
  std::string gate_why;
  std::string guard;
  std::string landing;
  bool landing_votes{false};
  std::string route;
  std::string worst_module;
  std::string worst;
  Px4Command px4;
};

inline Snapshot evaluate(const Cache& c, const Params& p) {
  Snapshot s;
  SupervisorState sup;
  sup.airborne = c.airborne;
  sup.armed = c.armed;
  sup.remote_id_ok = c.remote_id_ok;
  sup.gps_ok = c.gps_ok;
  sup.inside_fence = c.inside_fence;
  sup.detector_ok = c.detector_ok;
  sup.animal = c.lidar_kind == "animal" || c.thermal_kind == "animal";
  sup.batt_pct = c.soc_pct;
  sup.cell_temp_c = c.cell_temp_c;
  sup.wind_ms = c.wind_ms;
  sup.rain = c.rain;
  sup.alt_m = c.alt_m;
  sup.cover_open = c.cover_open;
  s.supervisor = action_name(decide(sup, p.supervisor));
  s.deploy = deploy_name(deploy(sup, p.supervisor));

  MissionState m;
  m.phase = c.phase;
  m.day = c.day;
  m.legal_area_ok = c.legal_area_ok;
  m.observer_ok = c.observer_ok;
  m.remote_id_ok = c.remote_id_ok;
  m.soc_pct = c.soc_pct;
  m.pack_wh = c.pack_wh > 0 ? c.pack_wh : p.mission_gate.default_pack_wh;
  m.dist_home_km = c.dist_home_km;
  m.wh_per_km = c.wh_per_km > 0 ? c.wh_per_km : p.mission_gate.wh_per_km;
  m.solar_forecast_wh = c.solar_forecast_wh;
  m.link_gap_s = c.link_gap_s;
  m.wind_ms = c.wind_ms;
  m.cell_temp_c = c.cell_temp_c;
  m.intrusion = c.intrusion || (c.phase == "OVERNIGHT" && (c.lidar_kind == "human" || c.thermal_kind == "human"));
  s.gate = gate_name(gate(m, p.mission_gate, s.gate_why));

  Detection lidar{c.lidar_kind, c.lidar_dist, c.lidar_closing, c.soc_pct, c.cover_open, c.wind_ok};
  Detection thermal{c.thermal_kind, c.thermal_dist, c.thermal_closing, c.soc_pct, c.cover_open, c.wind_ok};
  s.guard = worse_guard(guard_decide(lidar, p.guard), guard_decide(thermal, p.guard));

  const double soc_fraction = c.soc_pct > 1.0 ? c.soc_pct / 100.0 : c.soc_pct;
  const double leg = c.leg_km > 0 ? c.leg_km : p.landing.leg_km.value;
  auto leg_result = next_leg(soc_fraction, c.cell_temp_c, c.props_ok, c.geofence_ok, c.remote_id_ok, p.landing, leg);
  s.landing = leg_result.verdict;
  s.landing_votes = !c.airborne;

  const double dist = p.route.straight_line_mi * p.route.mi_to_km;
  auto route = plan_route(dist, p.route, p.route.real_flight, p.route.tribal_permission, p.route.faa_waiver,
                          c.legal_area_ok);
  s.route = route.verdict;

  std::vector<Vote> votes{
      {"supervisor", s.supervisor, true},
      {"mission_gate", s.gate, true},
      {"guard", s.guard, true},
      {"landing", s.landing, s.landing_votes},
      {"route", s.route, true},
  };
  auto worst = worst_vote(votes);
  s.worst_module = worst.first;
  s.worst = worst.second;

  std::vector<Px4Command> parts;
  parts.push_back(map_request(s.supervisor, c.airborne));
  parts.push_back(map_request(s.gate, c.airborne));
  parts.push_back(map_request(s.guard, c.airborne));
  if (s.landing_votes) parts.push_back(map_request(s.landing, c.airborne));
  parts.push_back(map_request(s.route, c.airborne));
  s.px4 = combine_requests(parts, c.airborne);
  return s;
}

}  // namespace drone_safety
