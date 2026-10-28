#pragma once

#include "drone_safety_ops/params.hpp"

#include <string>

namespace drone_safety {

struct LegResult {
  std::string verdict;
  double need_min{0};
  double have_min{0};
  double req_wh_per_h{0};
};

inline int leg_rank(const std::string& s) {
  if (s == "GO") return 0;
  if (s == "HOLD_GROUND") return 1;
  if (s == "NO_GO") return 2;
  return 0;
}

inline std::string worse_leg(const std::string& a, const std::string& b) {
  return leg_rank(a) >= leg_rank(b) ? a : b;
}

inline LegResult next_leg(double soc, double cell_c, bool props_ok, bool geofence_ok, bool rid_ok,
                          const Params::Landing& p, double leg_km) {
  std::string v = "GO";
  if (!props_ok || !geofence_ok || !rid_ok) v = worse_leg(v, "HOLD_GROUND");
  if (cell_c >= p.cell_hold_c || soc < p.soc_hold_fraction) v = worse_leg(v, "HOLD_GROUND");
  const double need = leg_km / p.cruise_kmh.value * 60.0 * p.energy_margin;
  const double have = p.batt_endurance_min.value * (1.0 - p.reserve_fraction) * (soc <= 1.0 ? soc : 1.0);
  if (need > have) v = worse_leg(v, "NO_GO");
  LegResult r;
  r.verdict = v;
  r.need_min = need;
  r.have_min = have;
  r.req_wh_per_h = leg_km / (p.batt_endurance_min.value / 60.0);
  return r;
}

}  // namespace drone_safety
