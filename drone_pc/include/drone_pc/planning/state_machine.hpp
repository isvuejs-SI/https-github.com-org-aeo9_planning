#pragma once

#include <string>
#include <vector>

namespace drone_pc {

enum class MissionPhase {
  SLEEP_CHARGING,
  PREFLIGHT_GATE,
  TAKEOFF,
  CRUISE,
  SURVEY,
  RETURN,
  LAND,
  ESCAPE,
};

std::string to_string(MissionPhase phase);
MissionPhase mission_phase_from_string(const std::string& name);

struct TransitionRecord {
  MissionPhase from;
  MissionPhase to;
  std::string reason;
};

class StateMachine {
 public:
  explicit StateMachine(MissionPhase initial = MissionPhase::SLEEP_CHARGING);

  MissionPhase current() const { return current_; }
  const std::vector<TransitionRecord>& history() const { return history_; }

  bool can_transition(MissionPhase to) const;
  bool transition(MissionPhase to, const std::string& reason);

 private:
  MissionPhase current_;
  std::vector<TransitionRecord> history_;
};

}  // namespace drone_pc
