#include "drone_pc/safety/preflight_gate.hpp"

#include <stdexcept>

namespace drone_pc {

std::string to_string(PreflightVerdict v) {
  switch (v) {
    case PreflightVerdict::GO:
      return "GO";
    case PreflightVerdict::HOLD_GROUND:
      return "HOLD_GROUND";
    case PreflightVerdict::RTL:
      return "RTL";
    case PreflightVerdict::ABORT_MISSION:
      return "ABORT_MISSION";
    case PreflightVerdict::LAND_NOW:
      return "LAND_NOW";
    case PreflightVerdict::NO_GO:
      return "NO_GO";
  }
  return "UNKNOWN";
}

PreflightVerdict preflight_verdict_from_string(const std::string& name) {
  if (name == "GO") return PreflightVerdict::GO;
  if (name == "HOLD_GROUND") return PreflightVerdict::HOLD_GROUND;
  if (name == "RTL") return PreflightVerdict::RTL;
  if (name == "ABORT_MISSION") return PreflightVerdict::ABORT_MISSION;
  if (name == "LAND_NOW") return PreflightVerdict::LAND_NOW;
  if (name == "NO_GO") return PreflightVerdict::NO_GO;
  throw std::invalid_argument("Unknown preflight verdict: " + name);
}

PreflightVerdict worse(PreflightVerdict a, PreflightVerdict b) {
  return static_cast<int>(a) >= static_cast<int>(b) ? a : b;
}

PreflightGate::PreflightGate(const Params& params) : params_(params) {}

PreflightResult PreflightGate::evaluate(const PreflightInputs& in) const {
  PreflightResult out;
  out.verdict = PreflightVerdict::GO;

  if (in.wind_mps > params_.preflight_max_wind_mps) {
    out.verdict = worse(out.verdict, in.airborne ? PreflightVerdict::RTL : PreflightVerdict::NO_GO);
    out.reasons.push_back("wind_exceeds_limit");
  }

  if (in.cell_temp_c > params_.preflight_max_cell_temp_c) {
    out.verdict =
        worse(out.verdict, in.airborne ? PreflightVerdict::LAND_NOW : PreflightVerdict::NO_GO);
    out.reasons.push_back("cell_temp_high");
  } else if (in.cell_temp_c < params_.preflight_min_cell_temp_c) {
    out.verdict =
        worse(out.verdict, in.airborne ? PreflightVerdict::RTL : PreflightVerdict::HOLD_GROUND);
    out.reasons.push_back("cell_temp_low");
  }

  if (in.soc_percent < params_.preflight_min_soc_percent) {
    out.verdict =
        worse(out.verdict, in.airborne ? PreflightVerdict::RTL : PreflightVerdict::HOLD_GROUND);
    out.reasons.push_back("soc_below_min");
  }

  if (params_.preflight_require_geofence_ok && !in.geofence_ok) {
    out.verdict =
        worse(out.verdict, in.airborne ? PreflightVerdict::ABORT_MISSION : PreflightVerdict::NO_GO);
    out.reasons.push_back("geofence_violation");
  }

  if (params_.preflight_require_remote_id && !in.remote_id_ok) {
    out.verdict =
        worse(out.verdict, in.airborne ? PreflightVerdict::LAND_NOW : PreflightVerdict::NO_GO);
    out.reasons.push_back("remote_id_failed");
  }

  if (params_.preflight_require_return_reserve && !in.return_reserve_ok) {
    out.verdict =
        worse(out.verdict, in.airborne ? PreflightVerdict::RTL : PreflightVerdict::HOLD_GROUND);
    out.reasons.push_back("return_reserve_insufficient");
  }

  return out;
}

}  // namespace drone_pc
