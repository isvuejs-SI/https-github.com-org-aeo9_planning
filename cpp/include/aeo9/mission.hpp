#pragma once

#include "aeo9/config.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace aeo9 {

struct LegEstimate {
  std::string name;
  std::string leg;
  int32_t north_cm{0};
  int32_t east_cm{0};
  int32_t alt_cm{0};
  int32_t energy_mwh{0};
};

struct DayPlan {
  std::vector<std::string> phases;
  std::vector<LegEstimate> legs;
  int32_t total_energy_mwh{0};
};

DayPlan plan_day(const Config& cfg);
std::vector<std::string> day_plan_dump_lines(const DayPlan& plan);

}  // namespace aeo9
