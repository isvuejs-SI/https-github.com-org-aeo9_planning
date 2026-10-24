#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace aeo9 {

class MockPx4Adapter {
 public:
  std::string arm();
  std::string takeoff(int32_t alt_cm);
  std::string goto_pos(int32_t north_cm, int32_t east_cm, int32_t alt_cm);
  std::string hold();
  std::string return_to_launch();
  std::string land();

 private:
  bool armed_{false};
  std::string mode_{"IDLE"};
  int32_t north_cm_{0};
  int32_t east_cm_{0};
  int32_t alt_cm_{0};
  std::vector<std::string> history_;
};

}  // namespace aeo9
