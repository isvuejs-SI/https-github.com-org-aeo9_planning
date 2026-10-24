#include "aeo9/ros2_bridge.hpp"

namespace aeo9 {

std::string MockRos2Bridge::publish_battery(int32_t percentage,
                                            int32_t voltage_mv) {
  return "ROS2_BATTERY pct=" + std::to_string(percentage) +
         " mv=" + std::to_string(voltage_mv);
}

std::string MockRos2Bridge::publish_image(int32_t width, int32_t height,
                                          int32_t /*stamp_ms*/, bool ok) {
  last_image_ok_ = ok;
  return "ROS2_IMAGE w=" + std::to_string(width) +
         " h=" + std::to_string(height) + " ok=" + std::to_string(ok ? 1 : 0);
}

std::string MockRos2Bridge::publish_status(const SupervisorStatus& status) {
  return "ROS2_STATUS state=" + status.state +
         " E=" + std::to_string(status.energy_mwh) +
         " alt=" + std::to_string(status.altitude_cm) +
         " film=" + std::to_string(status.filming ? 1 : 0);
}

std::string MockRos2Bridge::trigger_camera() {
  if (!last_image_ok_) return "CAMERA_TRIGGER_FAIL";
  camera_triggers_ += 1;
  return "CAMERA_TRIGGER n=" + std::to_string(camera_triggers_);
}

std::string MockRos2Bridge::set_starlink(bool up) {
  starlink_up_ = up;
  return std::string("STARLINK=") + (up ? "UP" : "DOWN");
}

}  // namespace aeo9
