#include "aeo9/px4_adapter.hpp"

namespace aeo9 {

std::string MockPx4Adapter::arm() {
  armed_ = true;
  mode_ = "ARMED";
  const std::string msg = "PX4_ARM";
  history_.push_back(msg);
  return msg;
}

std::string MockPx4Adapter::takeoff(int32_t alt_cm) {
  alt_cm_ = alt_cm;
  mode_ = "TAKEOFF";
  const std::string msg = "PX4_TAKEOFF alt_cm=" + std::to_string(alt_cm);
  history_.push_back(msg);
  return msg;
}

std::string MockPx4Adapter::goto_pos(int32_t north_cm, int32_t east_cm,
                                     int32_t alt_cm) {
  north_cm_ = north_cm;
  east_cm_ = east_cm;
  alt_cm_ = alt_cm;
  mode_ = "GOTO";
  const std::string msg = "PX4_GOTO n=" + std::to_string(north_cm) +
                          " e=" + std::to_string(east_cm) +
                          " alt=" + std::to_string(alt_cm);
  history_.push_back(msg);
  return msg;
}

std::string MockPx4Adapter::hold() {
  mode_ = "HOLD";
  const std::string msg = "PX4_HOLD";
  history_.push_back(msg);
  return msg;
}

std::string MockPx4Adapter::return_to_launch() {
  north_cm_ = 0;
  east_cm_ = 0;
  mode_ = "RTL";
  const std::string msg = "PX4_RTL";
  history_.push_back(msg);
  return msg;
}

std::string MockPx4Adapter::land() {
  alt_cm_ = 0;
  armed_ = false;
  mode_ = "LAND";
  const std::string msg = "PX4_LAND";
  history_.push_back(msg);
  return msg;
}

}  // namespace aeo9
