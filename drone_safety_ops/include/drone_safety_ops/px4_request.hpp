#pragma once

#include <map>
#include <string>
#include <vector>

namespace drone_safety {

// The Jetson sends only these requests. PX4 on the Pixhawk owns the final failsafe.
struct Px4Command {
  std::string wire;  // none, hold, rtl, land, no_arm
  int command{0};
  bool has_param1{false};
  double param1{0};
  bool has_param2{false};
  double param2{0};
  bool has_param3{false};
  double param3{0};
  bool latch_no_arm{false};
};

inline int request_rank(const std::string& wire) {
  if (wire == "hold") return 1;
  if (wire == "rtl") return 2;
  if (wire == "land") return 3;
  if (wire == "no_arm") return 4;
  return 0;
}

inline Px4Command fill_command(const std::string& wire, bool latch) {
  Px4Command c;
  c.wire = wire;
  c.latch_no_arm = latch;
  if (wire == "hold") {
    // MAV_CMD_DO_SET_MODE, PX4 AUTO.LOITER (main 4, sub 3).
    c.command = 176;
    c.has_param1 = c.has_param2 = c.has_param3 = true;
    c.param1 = 1;
    c.param2 = 4;
    c.param3 = 3;
  } else if (wire == "rtl") {
    c.command = 20;  // MAV_CMD_NAV_RETURN_TO_LAUNCH
  } else if (wire == "land") {
    c.command = 21;  // MAV_CMD_NAV_LAND
  } else if (wire == "no_arm") {
    c.command = 400;  // MAV_CMD_COMPONENT_ARM_DISARM
    c.has_param1 = true;
    c.param1 = 0;
  }
  return c;
}

inline bool latch_verdict(const std::string& verdict) {
  return verdict == "NO_TAKEOFF" || verdict == "NO_GO" || verdict == "PROP_LOCKOUT" || verdict == "HOLD_GROUND" ||
         verdict == "DETER";
}

// CLIMB_ESCAPE and ESCAPE are published and logged. They are not PX4 mode requests
// and this package never sends an arm.
inline Px4Command map_request(const std::string& verdict, bool airborne) {
  const bool latch = latch_verdict(verdict);
  if (verdict == "CONTINUE" || verdict == "GO" || verdict == "GO_SIM" || verdict == "STAY" || verdict == "WATCH" ||
      verdict == "ESCAPE" || verdict == "CLIMB_ESCAPE") {
    return fill_command("none", false);
  }
  if (airborne) {
    std::string wire = "none";
    if (verdict == "HOLD" || verdict == "HOLD_GROUND" || verdict == "DETER") wire = "hold";
    else if (verdict == "RTL" || verdict == "ABORT_MISSION") wire = "rtl";
    else if (verdict == "LAND" || verdict == "LAND_NOW" || verdict == "NO_TAKEOFF" || verdict == "NO_GO" ||
             verdict == "PROP_LOCKOUT")
      wire = "land";
    return fill_command(wire, latch);
  }
  if (verdict == "HOLD") return fill_command("hold", false);
  if (verdict == "RTL") return fill_command("rtl", false);
  if (verdict == "LAND" || verdict == "LAND_NOW") return fill_command("land", false);
  if (verdict == "ABORT_MISSION") return fill_command("no_arm", true);
  if (latch) return fill_command("no_arm", true);
  return fill_command("none", false);
}

inline Px4Command combine_requests(const std::vector<Px4Command>& parts, bool airborne) {
  std::string best = "none";
  bool latch = false;
  for (const auto& part : parts) {
    if (part.latch_no_arm) latch = true;
    if (request_rank(part.wire) > request_rank(best)) best = part.wire;
  }
  if (airborne && best == "no_arm") {
    best = "land";
    latch = true;
  }
  return fill_command(best, latch);
}

inline const char* mavsdk_action(const std::string& wire) {
  if (wire == "hold") return "action.hold";
  if (wire == "rtl") return "action.return_to_launch";
  if (wire == "land") return "action.land";
  if (wire == "no_arm") return "action.disarm";
  return "none";
}

inline std::string command_signature(const Px4Command& c) {
  std::string s = c.wire;
  if (c.wire == "none") return s;
  s += " cmd=" + std::to_string(c.command);
  if (c.has_param1) s += " p1=" + std::to_string(static_cast<int>(c.param1));
  if (c.has_param2) s += " p2=" + std::to_string(static_cast<int>(c.param2));
  if (c.has_param3) s += " p3=" + std::to_string(static_cast<int>(c.param3));
  return s;
}

}  // namespace drone_safety
