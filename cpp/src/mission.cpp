#include "aeo9/mission.hpp"

#include <cstdlib>

namespace aeo9 {
namespace {

int32_t distance_cm(const Waypoint& a, const Waypoint& b) {
  const int32_t abs_n = std::abs(b.north_cm - a.north_cm);
  const int32_t abs_e = std::abs(b.east_cm - a.east_cm);
  if (abs_n > abs_e) return abs_n + abs_e / 2;
  return abs_e + abs_n / 2;
}

int32_t leg_energy_mwh(const Config& cfg, const std::string& leg_type,
                       int32_t dist_cm) {
  const int32_t path = dist_cm / 100;
  if (leg_type == "launch") return cfg.hover_draw_mwh_per_s * 10 + path;
  if (leg_type == "transit") return cfg.transit_draw_mwh_per_s * 20 + path;
  if (leg_type == "filming") return cfg.filming_leg_mwh + path;
  if (leg_type == "return") return cfg.return_leg_mwh + path;
  if (leg_type == "overnight") return 0;
  return path;
}

}  // namespace

DayPlan plan_day(const Config& cfg) {
  DayPlan plan;
  plan.phases = {"launch", "transit", "filming", "return", "overnight"};
  if (cfg.waypoints.empty()) return plan;

  Waypoint prev = cfg.waypoints.front();
  for (const auto& wp : cfg.waypoints) {
    const int32_t dist = distance_cm(prev, wp);
    LegEstimate leg;
    leg.name = wp.name;
    leg.leg = wp.leg;
    leg.north_cm = wp.north_cm;
    leg.east_cm = wp.east_cm;
    leg.alt_cm = wp.alt_cm;
    leg.energy_mwh = leg_energy_mwh(cfg, wp.leg, dist);
    plan.legs.push_back(leg);
    plan.total_energy_mwh += leg.energy_mwh;
    prev = wp;
  }
  return plan;
}

std::vector<std::string> day_plan_dump_lines(const DayPlan& plan) {
  std::string phases;
  for (size_t i = 0; i < plan.phases.size(); ++i) {
    if (i) phases += ",";
    phases += plan.phases[i];
  }
  std::vector<std::string> lines;
  lines.push_back("phases=" + phases);
  lines.push_back("leg_count=" + std::to_string(plan.legs.size()));
  for (const auto& leg : plan.legs) {
    lines.push_back("leg name=" + leg.name + " type=" + leg.leg +
                    " n=" + std::to_string(leg.north_cm) +
                    " e=" + std::to_string(leg.east_cm) +
                    " alt=" + std::to_string(leg.alt_cm) +
                    " mwh=" + std::to_string(leg.energy_mwh));
  }
  lines.push_back("total_energy_mwh=" + std::to_string(plan.total_energy_mwh));
  return lines;
}

}  // namespace aeo9
