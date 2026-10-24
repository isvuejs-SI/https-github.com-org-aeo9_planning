#include "drone_pc/planning/state_machine.hpp"

#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace drone_pc {
namespace {

using Allowed = std::unordered_map<MissionPhase, std::unordered_set<MissionPhase>>;

const Allowed& allowed_transitions() {
  static const Allowed kAllowed = {
      {MissionPhase::SLEEP_CHARGING, {MissionPhase::PREFLIGHT_GATE, MissionPhase::ESCAPE}},
      {MissionPhase::PREFLIGHT_GATE,
       {MissionPhase::TAKEOFF, MissionPhase::SLEEP_CHARGING, MissionPhase::ESCAPE}},
      {MissionPhase::TAKEOFF, {MissionPhase::CRUISE, MissionPhase::LAND, MissionPhase::ESCAPE}},
      {MissionPhase::CRUISE,
       {MissionPhase::SURVEY, MissionPhase::RETURN, MissionPhase::ESCAPE, MissionPhase::LAND}},
      {MissionPhase::SURVEY, {MissionPhase::CRUISE, MissionPhase::RETURN, MissionPhase::ESCAPE}},
      {MissionPhase::RETURN, {MissionPhase::LAND, MissionPhase::ESCAPE}},
      {MissionPhase::LAND, {MissionPhase::SLEEP_CHARGING, MissionPhase::ESCAPE}},
      {MissionPhase::ESCAPE, {MissionPhase::RETURN, MissionPhase::LAND, MissionPhase::SLEEP_CHARGING}},
  };
  return kAllowed;
}

}  // namespace

std::string to_string(MissionPhase phase) {
  switch (phase) {
    case MissionPhase::SLEEP_CHARGING:
      return "SLEEP_CHARGING";
    case MissionPhase::PREFLIGHT_GATE:
      return "PREFLIGHT_GATE";
    case MissionPhase::TAKEOFF:
      return "TAKEOFF";
    case MissionPhase::CRUISE:
      return "CRUISE";
    case MissionPhase::SURVEY:
      return "SURVEY";
    case MissionPhase::RETURN:
      return "RETURN";
    case MissionPhase::LAND:
      return "LAND";
    case MissionPhase::ESCAPE:
      return "ESCAPE";
  }
  return "UNKNOWN";
}

MissionPhase mission_phase_from_string(const std::string& name) {
  if (name == "SLEEP_CHARGING") return MissionPhase::SLEEP_CHARGING;
  if (name == "PREFLIGHT_GATE") return MissionPhase::PREFLIGHT_GATE;
  if (name == "TAKEOFF") return MissionPhase::TAKEOFF;
  if (name == "CRUISE") return MissionPhase::CRUISE;
  if (name == "SURVEY") return MissionPhase::SURVEY;
  if (name == "RETURN") return MissionPhase::RETURN;
  if (name == "LAND") return MissionPhase::LAND;
  if (name == "ESCAPE") return MissionPhase::ESCAPE;
  throw std::invalid_argument("Unknown mission phase: " + name);
}

StateMachine::StateMachine(MissionPhase initial) : current_(initial) {}

bool StateMachine::can_transition(MissionPhase to) const {
  if (to == current_) return false;
  const auto& table = allowed_transitions();
  const auto it = table.find(current_);
  if (it == table.end()) return false;
  return it->second.count(to) > 0;
}

bool StateMachine::transition(MissionPhase to, const std::string& reason) {
  if (!can_transition(to)) {
    return false;
  }
  history_.push_back(TransitionRecord{current_, to, reason});
  current_ = to;
  return true;
}

}  // namespace drone_pc
