#pragma once

#include "drone_safety_ops/params.hpp"

#include <cctype>
#include <functional>
#include <map>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

namespace drone_safety {

inline const std::map<std::string, std::string>& alert_priorities() {
  static const std::map<std::string, std::string> prio{
      {"WATCH", "default"}, {"DETER", "high"}, {"ESCAPE", "urgent"}, {"PROP_LOCKOUT", "urgent"}};
  return prio;
}

inline bool topic_token_ok(const std::string& topic) {
  if (topic.empty() || topic.size() > 128) return false;
  for (unsigned char c : topic) {
    if (!(std::isalnum(c) || c == '_' || c == '-' || c == '.')) return false;
  }
  return true;
}

// Duplicate hold-back is 60 s for the same verdict. A new verdict still alerts.
// STAY never alerts and does not clear the previous verdict.
class Alerter {
 public:
  Alerter(double holdback_s, std::function<double()> clock) : holdback_s_(holdback_s), clock_(std::move(clock)) {}

  bool alert(const std::string& verdict, const std::string& msg, bool dry, std::string* line) {
    const double now = clock_();
    if (verdict == "STAY" || (verdict == last_ && now - last_t_ < holdback_s_)) return false;
    auto it = alert_priorities().find(verdict);
    if (it == alert_priorities().end()) return false;
    last_ = verdict;
    last_t_ = now;
    if (line) *line = "ALERT " + it->second + " " + verdict + " " + msg;
    if (dry) return true;
    return send_ && send_(verdict, msg, it->second);
  }

  void set_sender(std::function<bool(const std::string&, const std::string&, const std::string&)> send) {
    send_ = std::move(send);
  }

  static bool send_ntfy_curl(const std::string& host, const std::string& topic, const std::string& verdict,
                             const std::string& msg) {
    if (!topic_token_ok(topic) || !topic_token_ok(host)) return false;
    const pid_t pid = ::fork();
    if (pid < 0) return false;
    if (pid == 0) {
      const std::string title = "Title: Drone " + verdict;
      auto it = alert_priorities().find(verdict);
      const std::string pri = "Priority: " + (it == alert_priorities().end() ? std::string("default") : it->second);
      const std::string url = "https://" + host + "/" + topic;
      ::execlp("curl", "curl", "-s", "-m", "5", "-H", title.c_str(), "-H", pri.c_str(), "-d", msg.c_str(), url.c_str(),
               static_cast<char*>(nullptr));
      _exit(127);
    }
    int status = 0;
    if (::waitpid(pid, &status, 0) < 0) return false;
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
  }

 private:
  double holdback_s_;
  std::function<double()> clock_;
  std::function<bool(const std::string&, const std::string&, const std::string&)> send_;
  std::string last_;
  double last_t_{0};
};

}  // namespace drone_safety
