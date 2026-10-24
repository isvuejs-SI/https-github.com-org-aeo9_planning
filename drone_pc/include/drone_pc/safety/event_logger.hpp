#pragma once

#include <string>
#include <vector>

#include "drone_pc/params.hpp"

namespace drone_pc {

struct LogEvent {
  double timestamp_s{0.0};
  std::string category;  // verdict | transition
  std::string payload;
};

class EventLogger {
 public:
  explicit EventLogger(const Params& params);

  void log_verdict(double timestamp_s, const std::string& payload);
  void log_transition(double timestamp_s, const std::string& payload);

  const std::vector<LogEvent>& events() const { return events_; }
  std::string directory() const { return params_.logging_log_directory; }

  // Persist in-memory buffer to a single text file (SITL-safe local storage)
  bool flush_to_file(const std::string& filename) const;

 private:
  Params params_;
  std::vector<LogEvent> events_;
};

}  // namespace drone_pc
