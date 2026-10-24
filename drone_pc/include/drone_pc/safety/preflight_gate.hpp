#pragma once

#include <string>
#include <vector>

#include "drone_pc/params.hpp"

namespace drone_pc {

// Worse wins: GO < HOLD_GROUND < RTL < ABORT_MISSION < LAND_NOW < NO_GO
enum class PreflightVerdict {
  GO = 0,
  HOLD_GROUND = 1,
  RTL = 2,
  ABORT_MISSION = 3,
  LAND_NOW = 4,
  NO_GO = 5,
};

std::string to_string(PreflightVerdict v);
PreflightVerdict preflight_verdict_from_string(const std::string& name);
PreflightVerdict worse(PreflightVerdict a, PreflightVerdict b);

struct PreflightInputs {
  double wind_mps{0.0};
  double cell_temp_c{25.0};
  double soc_percent{100.0};
  bool geofence_ok{true};
  bool remote_id_ok{true};
  bool return_reserve_ok{true};
  bool airborne{false};
};

struct PreflightResult {
  PreflightVerdict verdict{PreflightVerdict::GO};
  std::vector<std::string> reasons;
};

class PreflightGate {
 public:
  explicit PreflightGate(const Params& params);

  PreflightResult evaluate(const PreflightInputs& in) const;

 private:
  Params params_;
};

}  // namespace drone_pc
