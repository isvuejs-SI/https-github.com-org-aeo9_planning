#pragma once

#include <string>

#include "drone_pc/params.hpp"
#include "drone_pc/planning/occupancy_map.hpp"

namespace drone_pc {

// Consumed by escape planner for animal/human near parked drone
enum class EscapeVerdict {
  STAY,
  WATCH,
  DETER,
  ESCAPE,
  PROP_LOCKOUT,
};

std::string to_string(EscapeVerdict v);
EscapeVerdict escape_verdict_from_string(const std::string& name);

struct EscapeInput {
  EscapeVerdict threat_verdict{EscapeVerdict::STAY};
  double soc_percent{100.0};
  bool cover_open{true};
  bool wind_ok{true};
  Point3 current_position;
  Point3 backup_site;
  bool backup_valid{false};
};

struct EscapeOutput {
  EscapeVerdict action{EscapeVerdict::STAY};
  bool execute_escape{false};
  Point3 target;
  std::string reason;
};

class EscapePlanner {
 public:
  explicit EscapePlanner(const Params& params);

  EscapeVerdict classify_threat(double distance_m) const;
  EscapeOutput plan(const EscapeInput& in) const;

 private:
  Params params_;
};

}  // namespace drone_pc
