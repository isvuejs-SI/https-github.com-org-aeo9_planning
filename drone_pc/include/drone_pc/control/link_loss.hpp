#pragma once

#include <string>

#include "drone_pc/params.hpp"

namespace drone_pc {

enum class LinkLossAction {
  NONE,
  RTL,
  ABORT_MISSION,
};

std::string to_string(LinkLossAction a);

struct LinkLossResult {
  LinkLossAction action{LinkLossAction::NONE};
  double disconnected_s{0.0};
  std::string reason;
};

class LinkLossMonitor {
 public:
  explicit LinkLossMonitor(const Params& params);

  // link_ok=true resets the timer; false accumulates disconnected time.
  LinkLossResult update(double dt_s, bool link_ok);

  double disconnected_s() const { return disconnected_s_; }

 private:
  Params params_;
  double disconnected_s_{0.0};
};

}  // namespace drone_pc
