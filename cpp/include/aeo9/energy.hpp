#pragma once

#include <cstdint>
#include <string>
#include <utility>

namespace aeo9 {

inline int32_t apply_energy_step(int32_t energy_mwh, int32_t capacity_mwh,
                                 int32_t draw_mwh, int32_t solar_mwh) {
  int32_t next = energy_mwh - draw_mwh + solar_mwh;
  if (next < 0) next = 0;
  if (next > capacity_mwh) next = capacity_mwh;
  return next;
}

inline std::pair<bool, std::string> launch_gate(int32_t energy_mwh,
                                                int32_t energy_need_mwh,
                                                int32_t reserve_mwh) {
  const int32_t threshold = energy_need_mwh + reserve_mwh;
  if (energy_mwh >= threshold) {
    return {true, "LAUNCH_OK E=" + std::to_string(energy_mwh) +
                      " need+reserve=" + std::to_string(threshold)};
  }
  return {false, "LAUNCH_BLOCKED E=" + std::to_string(energy_mwh) +
                     " need+reserve=" + std::to_string(threshold)};
}

}  // namespace aeo9
