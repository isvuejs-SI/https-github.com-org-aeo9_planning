#pragma once

#include <cstdint>
#include <string>

namespace aeo9 {

enum class GateResult { GO, WAIT, SKIP };

struct GateDecision {
  GateResult result;
  std::string reason;
};

GateDecision evaluate_go_nogo(int32_t wind_cm_s, int32_t max_wind_cm_s,
                              int32_t energy_mwh, int32_t energy_need_mwh,
                              int32_t reserve_mwh, bool schedule_ok,
                              bool inside_fence);

std::string gate_dump_line(const GateDecision& d);

}  // namespace aeo9
