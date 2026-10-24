#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "drone_pc/params.hpp"

namespace drone_pc {

struct AlertRecord {
  std::string key;
  std::string message;
  double timestamp_s{0.0};
  bool sent{false};
  std::string reason;
};

// Phone alert via ntfy with duplicate hold-back. Transport is stubbed for SITL.
class AlertManager {
 public:
  explicit AlertManager(const Params& params);

  AlertRecord maybe_send(const std::string& key, const std::string& message, double now_s);

  const std::vector<AlertRecord>& history() const { return history_; }

  // Builds the HTTP URL that would be used (no network in unit tests)
  std::string ntfy_url() const;

 private:
  Params params_;
  std::unordered_map<std::string, double> last_sent_s_;
  std::vector<AlertRecord> history_;
};

}  // namespace drone_pc
