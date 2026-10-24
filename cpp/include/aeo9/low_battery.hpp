#pragma once

#include <cstdint>
#include <string>
#include <utility>

namespace aeo9 {

inline std::pair<bool, std::string> should_return_low_battery(
    int32_t energy_mwh, int32_t reserve_mwh, int32_t return_cost_mwh) {
  const int32_t threshold = reserve_mwh + return_cost_mwh;
  if (energy_mwh <= threshold) {
    return {true, "LOW_BATTERY_RTL E=" + std::to_string(energy_mwh) +
                      "<=" + std::to_string(threshold)};
  }
  return {false, "BATTERY_OK E=" + std::to_string(energy_mwh) + ">" +
                     std::to_string(threshold)};
}

}  // namespace aeo9
