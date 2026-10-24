#include "aeo9/go_nogo.hpp"

namespace aeo9 {

GateDecision evaluate_go_nogo(int32_t wind_cm_s, int32_t max_wind_cm_s,
                              int32_t energy_mwh, int32_t energy_need_mwh,
                              int32_t reserve_mwh, bool schedule_ok,
                              bool inside_fence) {
  if (wind_cm_s > max_wind_cm_s) {
    return {GateResult::WAIT, "wind=" + std::to_string(wind_cm_s) + ">" +
                                  std::to_string(max_wind_cm_s)};
  }
  const int32_t threshold = energy_need_mwh + reserve_mwh;
  if (energy_mwh < threshold) {
    return {GateResult::WAIT, "battery E=" + std::to_string(energy_mwh) + "<" +
                                  std::to_string(threshold)};
  }
  if (!schedule_ok) {
    return {GateResult::SKIP, "schedule_closed"};
  }
  if (!inside_fence) {
    return {GateResult::SKIP, "outside_fence"};
  }
  return {GateResult::GO, "all_checks_passed"};
}

std::string gate_dump_line(const GateDecision& d) {
  const char* name = "GO";
  if (d.result == GateResult::WAIT) name = "WAIT";
  if (d.result == GateResult::SKIP) name = "SKIP";
  return std::string("go_nogo=") + name + " reason=" + d.reason;
}

}  // namespace aeo9
