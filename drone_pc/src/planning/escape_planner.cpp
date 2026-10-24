#include "drone_pc/planning/escape_planner.hpp"

#include <stdexcept>

namespace drone_pc {

std::string to_string(EscapeVerdict v) {
  switch (v) {
    case EscapeVerdict::STAY:
      return "STAY";
    case EscapeVerdict::WATCH:
      return "WATCH";
    case EscapeVerdict::DETER:
      return "DETER";
    case EscapeVerdict::ESCAPE:
      return "ESCAPE";
    case EscapeVerdict::PROP_LOCKOUT:
      return "PROP_LOCKOUT";
  }
  return "UNKNOWN";
}

EscapeVerdict escape_verdict_from_string(const std::string& name) {
  if (name == "STAY") return EscapeVerdict::STAY;
  if (name == "WATCH") return EscapeVerdict::WATCH;
  if (name == "DETER") return EscapeVerdict::DETER;
  if (name == "ESCAPE") return EscapeVerdict::ESCAPE;
  if (name == "PROP_LOCKOUT") return EscapeVerdict::PROP_LOCKOUT;
  throw std::invalid_argument("Unknown escape verdict: " + name);
}

EscapePlanner::EscapePlanner(const Params& params) : params_(params) {}

EscapeVerdict EscapePlanner::classify_threat(double distance_m) const {
  if (distance_m <= params_.escape_prop_lockout_radius_m) return EscapeVerdict::PROP_LOCKOUT;
  if (distance_m <= params_.escape_escape_radius_m) return EscapeVerdict::ESCAPE;
  if (distance_m <= params_.escape_deter_radius_m) return EscapeVerdict::DETER;
  if (distance_m <= params_.escape_watch_radius_m) return EscapeVerdict::WATCH;
  return EscapeVerdict::STAY;
}

EscapeOutput EscapePlanner::plan(const EscapeInput& in) const {
  EscapeOutput out;
  out.action = in.threat_verdict;
  out.target = in.current_position;

  if (in.threat_verdict == EscapeVerdict::STAY || in.threat_verdict == EscapeVerdict::WATCH ||
      in.threat_verdict == EscapeVerdict::DETER) {
    out.execute_escape = false;
    out.reason = "no_escape_required";
    return out;
  }

  if (in.threat_verdict == EscapeVerdict::PROP_LOCKOUT) {
    out.execute_escape = false;
    out.reason = "prop_lockout_hold";
    return out;
  }

  // ESCAPE: climb and move to backup only if battery ≥ 30%, cover open, wind OK
  if (in.soc_percent < params_.energy_escape_min_soc_percent) {
    out.execute_escape = false;
    out.reason = "escape_blocked_soc";
    return out;
  }
  if (!in.cover_open) {
    out.execute_escape = false;
    out.reason = "escape_blocked_cover";
    return out;
  }
  if (!in.wind_ok) {
    out.execute_escape = false;
    out.reason = "escape_blocked_wind";
    return out;
  }
  if (!in.backup_valid) {
    out.execute_escape = false;
    out.reason = "escape_blocked_no_backup";
    return out;
  }

  out.execute_escape = true;
  out.target = in.backup_site;
  out.target.z = params_.escape_climb_altitude_m;
  out.reason = "escape_to_backup";
  return out;
}

}  // namespace drone_pc
