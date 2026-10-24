#pragma once

#include <cstdint>
#include <string>

namespace aeo9 {

struct SupervisorStatus {
  std::string state;
  int32_t energy_mwh{0};
  int32_t altitude_cm{0};
  bool filming{false};
};

class MockRos2Bridge {
 public:
  std::string publish_battery(int32_t percentage, int32_t voltage_mv);
  std::string publish_image(int32_t width, int32_t height, int32_t stamp_ms,
                            bool ok);
  std::string publish_status(const SupervisorStatus& status);
  std::string trigger_camera();
  std::string set_starlink(bool up);

 private:
  bool last_image_ok_{true};
  int32_t camera_triggers_{0};
  bool starlink_up_{true};
};

}  // namespace aeo9
