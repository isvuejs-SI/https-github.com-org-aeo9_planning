#pragma once

#include "drone_safety_ops/params.hpp"

#include <string>

namespace drone_safety {

enum class Action : int { CONTINUE = 0, HOLD = 1, CLIMB_ESCAPE = 2, RTL = 3, LAND = 4, NO_TAKEOFF = 5 };

inline const char* action_name(Action a) {
  switch (a) {
    case Action::CONTINUE: return "CONTINUE";
    case Action::HOLD: return "HOLD";
    case Action::CLIMB_ESCAPE: return "CLIMB_ESCAPE";
    case Action::RTL: return "RTL";
    case Action::LAND: return "LAND";
    case Action::NO_TAKEOFF: return "NO_TAKEOFF";
  }
  return "?";
}

enum class Deploy : int { BLOCKED = 0, GEAR_DOWN = 1, OPEN_COVER = 2, DEPLOY_PANELS = 3 };

inline const char* deploy_name(Deploy d) {
  switch (d) {
    case Deploy::BLOCKED: return "BLOCKED";
    case Deploy::GEAR_DOWN: return "GEAR_DOWN";
    case Deploy::OPEN_COVER: return "OPEN_COVER";
    case Deploy::DEPLOY_PANELS: return "DEPLOY_PANELS";
  }
  return "?";
}

struct SupervisorState {
  std::string scene;
  bool airborne{false};
  bool armed{false};
  bool remote_id_ok{false};
  bool gps_ok{false};
  bool inside_fence{false};
  bool detector_ok{false};
  bool animal{false};
  double batt_pct{0};
  double cell_temp_c{0};
  double wind_ms{0};
  bool rain{false};
  double alt_m{0};
  bool cover_open{false};
};

inline Action decide(const SupervisorState& s, const Params::Supervisor& p) {
  Action w = Action::CONTINUE;
  auto up = [&](Action n) {
    if (n > w) w = n;
  };
  if (!s.airborne) {
    if (!s.remote_id_ok || !s.gps_ok || s.batt_pct < p.batt_no_takeoff_pct || s.cell_temp_c > p.cell_temp_limit_c)
      up(Action::NO_TAKEOFF);
    return w;
  }
  if (!s.detector_ok) up(Action::HOLD);
  if (s.animal) up(Action::CLIMB_ESCAPE);
  if (!s.inside_fence) up(Action::RTL);
  if (s.batt_pct < p.batt_rtl_pct) up(Action::RTL);
  if (!s.gps_ok) up(Action::LAND);
  if (s.batt_pct < p.batt_land_pct || s.cell_temp_c > p.cell_temp_limit_c) up(Action::LAND);
  return w;
}

inline Deploy deploy(const SupervisorState& s, const Params::Supervisor& p) {
  if (s.airborne) return s.alt_m < p.gear_down_alt_m ? Deploy::GEAR_DOWN : Deploy::BLOCKED;
  if (s.armed || s.wind_ms > p.deploy_wind_max_ms || s.rain) return Deploy::BLOCKED;
  return s.cover_open ? Deploy::DEPLOY_PANELS : Deploy::OPEN_COVER;
}

}  // namespace drone_safety
