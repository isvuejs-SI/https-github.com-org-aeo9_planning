#pragma once

#include "drone_safety_ops/params.hpp"

#include <cmath>
#include <string>

namespace drone_safety {

struct RouteResult {
  std::string verdict;
  double one_pack_km{0};
  int legs{0};
  double req{0};
};

// Real flight stays NO_GO unless tribal permission and the Part 107 waiver
// are both set, the park is clear, and the relayed leg fits in one pack.
// Simulation with real_flight false can return GO_SIM only.
inline RouteResult plan_route(double dist_km, const Params::Route& p, bool real_flight, bool tribal_ok,
                              bool waiver, bool park_clear) {
  const double pack =
      p.batt_endurance_min.value * (1.0 - p.reserve_fraction) / p.energy_margin * p.cruise_kmh.value / 60.0;
  RouteResult r;
  r.one_pack_km = pack;
  r.legs = static_cast<int>(std::ceil(dist_km / pack));
  r.req = p.leg_km.value / (p.batt_endurance_min.value / 60.0);
  r.verdict = "GO_SIM";
  if (!park_clear) r.verdict = "NO_GO";
  if (real_flight && (!tribal_ok || !waiver)) r.verdict = "NO_GO";
  if (p.leg_km.value > pack && real_flight) r.verdict = "NO_GO";
  return r;
}

}  // namespace drone_safety
