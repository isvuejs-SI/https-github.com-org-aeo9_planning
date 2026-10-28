#pragma once

#include "drone_safety_ops/params.hpp"

#include <string>
#include <vector>

namespace drone_safety {

enum class GateVerdict : int {
  GO = 0,
  HOLD_GROUND = 1,
  RTL = 2,
  ABORT_MISSION = 3,
  LAND_NOW = 4,
  NO_GO = 5
};

inline const char* gate_name(GateVerdict v) {
  switch (v) {
    case GateVerdict::GO: return "GO";
    case GateVerdict::HOLD_GROUND: return "HOLD_GROUND";
    case GateVerdict::RTL: return "RTL";
    case GateVerdict::ABORT_MISSION: return "ABORT_MISSION";
    case GateVerdict::LAND_NOW: return "LAND_NOW";
    case GateVerdict::NO_GO: return "NO_GO";
  }
  return "?";
}

struct MissionState {
  std::string name;
  std::string phase;
  int day{1};
  bool legal_area_ok{false};
  bool observer_ok{false};
  bool remote_id_ok{false};
  double soc_pct{0};
  double pack_wh{0};
  double dist_home_km{0};
  double wh_per_km{0};
  double solar_forecast_wh{0};
  double link_gap_s{0};
  double wind_ms{0};
  double cell_temp_c{0};
  bool intrusion{false};
};

inline double return_wh(const MissionState& m, const Params::Mission& p) {
  return m.dist_home_km * m.wh_per_km * p.energy_margin + p.reserve_fraction * m.pack_wh;
}

inline GateVerdict gate(const MissionState& m, const Params::Mission& p, std::string& why) {
  GateVerdict v = GateVerdict::GO;
  why.clear();
  auto up = [&](GateVerdict n, const char* reason) {
    if (n > v) v = n;
    if (n > GateVerdict::GO) {
      if (!why.empty()) why += "; ";
      why += reason;
    }
  };
  const double have = m.soc_pct / 100.0 * m.pack_wh;
  const bool night = m.phase == "OVERNIGHT";
  if (!m.legal_area_ok) up(GateVerdict::NO_GO, "outside legal area");
  if ((m.phase == "LAUNCH" || night) && !m.remote_id_ok) up(GateVerdict::NO_GO, "remote id");
  if (!night && !m.observer_ok) up(GateVerdict::RTL, "no visual observer");
  if (m.day > p.max_days) up(GateVerdict::ABORT_MISSION, "past day budget");
  if (night) {
    if (m.intrusion) up(GateVerdict::HOLD_GROUND, "intrusion: stay locked");
    if (have + m.solar_forecast_wh < return_wh(m, p)) up(GateVerdict::ABORT_MISSION, "solar cannot cover return");
    if (m.wind_ms > p.overnight_wind_hold_ms) up(GateVerdict::HOLD_GROUND, "wind: panels stowed");
  } else {
    if (have < return_wh(m, p)) up(GateVerdict::RTL, "return reserve");
    if (m.link_gap_s > p.link_rtl_s) up(GateVerdict::RTL, "link gap");
    if (m.link_gap_s > p.link_abort_s) up(GateVerdict::ABORT_MISSION, "link lost");
    if (m.wind_ms > p.flight_wind_rtl_ms) up(GateVerdict::RTL, "wind");
  }
  if (m.cell_temp_c > p.cell_overtemp_c)
    up(night ? GateVerdict::ABORT_MISSION : GateVerdict::LAND_NOW, "cell overtemp");
  if (why.empty()) why = "ok";
  return v;
}

}  // namespace drone_safety
