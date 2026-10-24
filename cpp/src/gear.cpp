#include "aeo9/gear.hpp"

namespace aeo9 {

std::pair<GearActionResult, std::string> GearController::stow() {
  stowed_ = true;
  return {GearActionResult::OK, "GEAR_STOWED"};
}

std::pair<GearActionResult, std::string> GearController::arm() {
  if (!stowed_) {
    return {GearActionResult::REFUSED, "ARM_REFUSED_GEAR_DEPLOYED"};
  }
  armed_ = true;
  landed_ = false;
  return {GearActionResult::OK, "ARMED"};
}

std::pair<GearActionResult, std::string> GearController::deploy() {
  if (!landed_) {
    return {GearActionResult::REFUSED, "DEPLOY_REFUSED_NOT_LANDED"};
  }
  if (armed_) {
    return {GearActionResult::REFUSED, "DEPLOY_REFUSED_STILL_ARMED"};
  }
  stowed_ = false;
  return {GearActionResult::OK, "GEAR_DEPLOYED"};
}

std::pair<GearActionResult, std::string> GearController::land() {
  landed_ = true;
  armed_ = false;
  return {GearActionResult::OK, "LANDED"};
}

std::string GearController::dump_line() const {
  return "gear stowed=" + std::to_string(stowed_ ? 1 : 0) +
         " armed=" + std::to_string(armed_ ? 1 : 0) +
         " landed=" + std::to_string(landed_ ? 1 : 0);
}

}  // namespace aeo9
