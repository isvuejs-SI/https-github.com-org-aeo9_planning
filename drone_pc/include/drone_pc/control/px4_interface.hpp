#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "drone_pc/params.hpp"
#include "drone_pc/planning/occupancy_map.hpp"

namespace drone_pc {

// Jetson sends requests only. PX4 owns final failsafes.
enum class Px4Request {
  NONE,
  ARM,
  TAKEOFF,
  HOLD,
  RTL,
  LAND,
  OFFBOARD_SETPOINT,
};

std::string to_string(Px4Request r);

struct OffboardSetpoint {
  Point3 position;
  double yaw_rad{0.0};
};

struct Px4InterfaceStatus {
  bool heartbeat_ok{true};
  int missed_heartbeats{0};
  Px4Request last_request{Px4Request::NONE};
  std::string fallback;  // hold | rtl when heartbeat lost (PX4-side expectation)
};

class Px4Interface {
 public:
  explicit Px4Interface(const Params& params);

  void tick(double dt_s);
  void send_heartbeat();
  bool request(Px4Request req);
  bool send_offboard_setpoint(const OffboardSetpoint& sp);

  double offboard_period_s() const;
  const Px4InterfaceStatus& status() const { return status_; }
  const std::vector<std::string>& request_log() const { return request_log_; }

 private:
  Params params_;
  Px4InterfaceStatus status_;
  double time_since_hb_{0.0};
  std::vector<std::string> request_log_;
};

}  // namespace drone_pc
