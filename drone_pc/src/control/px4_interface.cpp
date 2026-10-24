#include "drone_pc/control/px4_interface.hpp"

#include <stdexcept>

namespace drone_pc {

std::string to_string(Px4Request r) {
  switch (r) {
    case Px4Request::NONE:
      return "NONE";
    case Px4Request::ARM:
      return "ARM";
    case Px4Request::TAKEOFF:
      return "TAKEOFF";
    case Px4Request::HOLD:
      return "HOLD";
    case Px4Request::RTL:
      return "RTL";
    case Px4Request::LAND:
      return "LAND";
    case Px4Request::OFFBOARD_SETPOINT:
      return "OFFBOARD_SETPOINT";
  }
  return "UNKNOWN";
}

Px4Interface::Px4Interface(const Params& params) : params_(params) {}

double Px4Interface::offboard_period_s() const {
  return 1.0 / params_.control_offboard_rate_hz;
}

void Px4Interface::send_heartbeat() {
  time_since_hb_ = 0.0;
  status_.missed_heartbeats = 0;
  status_.heartbeat_ok = true;
  status_.fallback.clear();
}

void Px4Interface::tick(double dt_s) {
  time_since_hb_ += dt_s;
  if (time_since_hb_ > params_.control_heartbeat_period_s) {
    const int misses =
        static_cast<int>(time_since_hb_ / params_.control_heartbeat_period_s);
    status_.missed_heartbeats = misses;
    if (misses >= params_.control_heartbeat_miss_limit) {
      status_.heartbeat_ok = false;
      // Document expected PX4 fallback — Jetson does not override failsafes
      status_.fallback = "hold_or_rtl";
    }
  }
}

bool Px4Interface::request(Px4Request req) {
  // Requests only: arm, takeoff, hold, RTL, land (and offboard setpoints)
  switch (req) {
    case Px4Request::ARM:
    case Px4Request::TAKEOFF:
    case Px4Request::HOLD:
    case Px4Request::RTL:
    case Px4Request::LAND:
      status_.last_request = req;
      request_log_.push_back(to_string(req));
      return true;
    default:
      return false;
  }
}

bool Px4Interface::send_offboard_setpoint(const OffboardSetpoint& sp) {
  (void)sp;
  status_.last_request = Px4Request::OFFBOARD_SETPOINT;
  request_log_.push_back("OFFBOARD_SETPOINT");
  send_heartbeat();
  return true;
}

}  // namespace drone_pc
