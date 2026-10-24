#include "drone_pc/safety/event_logger.hpp"

#include <fstream>

namespace drone_pc {

EventLogger::EventLogger(const Params& params) : params_(params) {}

void EventLogger::log_verdict(double timestamp_s, const std::string& payload) {
  if (!params_.logging_log_verdicts) return;
  events_.push_back(LogEvent{timestamp_s, "verdict", payload});
}

void EventLogger::log_transition(double timestamp_s, const std::string& payload) {
  if (!params_.logging_log_transitions) return;
  events_.push_back(LogEvent{timestamp_s, "transition", payload});
}

bool EventLogger::flush_to_file(const std::string& filename) const {
  std::ofstream out(filename, std::ios::app);
  if (!out) return false;
  for (const auto& e : events_) {
    out << e.timestamp_s << '\t' << e.category << '\t' << e.payload << '\n';
  }
  return true;
}

}  // namespace drone_pc
