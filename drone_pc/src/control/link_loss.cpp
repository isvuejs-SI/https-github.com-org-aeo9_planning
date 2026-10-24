#include "drone_pc/control/link_loss.hpp"

namespace drone_pc {

std::string to_string(LinkLossAction a) {
  switch (a) {
    case LinkLossAction::NONE:
      return "NONE";
    case LinkLossAction::RTL:
      return "RTL";
    case LinkLossAction::ABORT_MISSION:
      return "ABORT_MISSION";
  }
  return "UNKNOWN";
}

LinkLossMonitor::LinkLossMonitor(const Params& params) : params_(params) {}

LinkLossResult LinkLossMonitor::update(double dt_s, bool link_ok) {
  LinkLossResult out;
  if (link_ok) {
    disconnected_s_ = 0.0;
    out.disconnected_s = 0.0;
    out.action = LinkLossAction::NONE;
    out.reason = "link_ok";
    return out;
  }

  disconnected_s_ += dt_s;
  out.disconnected_s = disconnected_s_;

  if (disconnected_s_ > params_.link_loss_abort_after_s) {
    out.action = LinkLossAction::ABORT_MISSION;
    out.reason = "link_loss_abort";
  } else if (disconnected_s_ > params_.link_loss_rtl_after_s) {
    out.action = LinkLossAction::RTL;
    out.reason = "link_loss_rtl";
  } else {
    out.action = LinkLossAction::NONE;
    out.reason = "link_loss_pending";
  }
  return out;
}

}  // namespace drone_pc
