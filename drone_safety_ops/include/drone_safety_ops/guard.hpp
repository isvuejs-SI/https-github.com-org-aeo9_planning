#pragma once

#include "drone_safety_ops/params.hpp"

#include <string>

namespace drone_safety {

struct Detection {
  std::string kind;
  double dist{0};
  double closing{0};
  double soc{0};
  bool cover_open{false};
  bool wind_ok{false};
};

// PROP_LOCKOUT means never arm. The distance test stays strict: inside 5 m.
inline std::string guard_decide(const Detection& s, const Params::Guard& p) {
  if (s.kind == "none") return "STAY";
  if (s.kind == "human" && s.dist < p.human_lockout_m) return "PROP_LOCKOUT";
  if (s.dist > p.watch_beyond_m) return "WATCH";
  if (s.kind == "human") return "DETER";
  if (s.dist < p.animal_escape_within_m && s.closing > p.animal_closing_mps) {
    const bool ok = s.soc >= p.escape_min_soc_pct && s.cover_open && s.wind_ok;
    return ok ? "ESCAPE" : "DETER";
  }
  return "DETER";
}

inline int guard_rank(const std::string& v) {
  if (v == "STAY") return 0;
  if (v == "WATCH") return 1;
  if (v == "DETER") return 2;
  if (v == "ESCAPE") return 3;
  if (v == "PROP_LOCKOUT") return 4;
  return 0;
}

inline std::string worse_guard(const std::string& a, const std::string& b) {
  return guard_rank(a) >= guard_rank(b) ? a : b;
}

}  // namespace drone_safety
