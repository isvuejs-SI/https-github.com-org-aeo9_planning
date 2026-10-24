#include "drone_pc/safety/alerts.hpp"

namespace drone_pc {

AlertManager::AlertManager(const Params& params) : params_(params) {}

std::string AlertManager::ntfy_url() const {
  return params_.alerts_ntfy_base_url + "/" + params_.alerts_ntfy_topic;
}

AlertRecord AlertManager::maybe_send(const std::string& key, const std::string& message,
                                     double now_s) {
  AlertRecord rec;
  rec.key = key;
  rec.message = message;
  rec.timestamp_s = now_s;

  if (!params_.alerts_enabled) {
    rec.sent = false;
    rec.reason = "alerts_disabled";
    history_.push_back(rec);
    return rec;
  }

  const auto it = last_sent_s_.find(key);
  if (it != last_sent_s_.end() &&
      (now_s - it->second) < params_.alerts_duplicate_holdback_s) {
    rec.sent = false;
    rec.reason = "duplicate_holdback";
    history_.push_back(rec);
    return rec;
  }

  // Stub: do not perform network I/O in SITL unit tests
  last_sent_s_[key] = now_s;
  rec.sent = true;
  rec.reason = "sent_stub";
  history_.push_back(rec);
  return rec;
}

}  // namespace drone_pc
