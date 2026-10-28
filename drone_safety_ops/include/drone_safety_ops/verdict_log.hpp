#pragma once

#include <cstdio>
#include <fstream>
#include <map>
#include <string>
#include <sys/stat.h>

namespace drone_safety {

struct LogRecord {
  std::string ts;
  std::string module;
  std::string verdict;
  std::string previous;
  bool transition{false};
  std::string px4;
  std::string reasons;
};

inline std::string json_escape(const std::string& in) {
  std::string out;
  for (char c : in) {
    if (c == '"' || c == '\\') continue;
    if (c == '\n' || c == '\r') continue;
    out.push_back(c);
  }
  return out;
}

inline std::string format_log_record(const LogRecord& r) {
  return std::string("{\"ts\":\"") + r.ts + "\",\"module\":\"" + r.module + "\",\"verdict\":\"" + r.verdict +
         "\",\"previous\":\"" + r.previous + "\",\"transition\":" + (r.transition ? "true" : "false") +
         ",\"px4\":\"" + r.px4 + "\",\"reasons\":\"" + json_escape(r.reasons) + "\"}";
}

class VerdictLog {
 public:
  explicit VerdictLog(std::string directory) : directory_(std::move(directory)) {
    ::mkdir(directory_.c_str(), 0755);
    path_ = directory_ + "/verdicts.jsonl";
  }

  const std::string& path() const { return path_; }

  LogRecord append(const std::string& ts, const std::string& module, const std::string& verdict,
                   const std::string& px4, const std::string& reasons) {
    LogRecord r;
    r.ts = ts;
    r.module = module;
    r.verdict = verdict;
    auto it = last_.find(module);
    r.previous = it == last_.end() ? "" : it->second;
    r.transition = it == last_.end() || it->second != verdict;
    r.px4 = px4;
    r.reasons = reasons;
    last_[module] = verdict;
    std::ofstream out(path_, std::ios::app);
    out << format_log_record(r) << "\n";
    return r;
  }

 private:
  std::string directory_;
  std::string path_;
  std::map<std::string, std::string> last_;
};

inline bool directory_writable(const std::string& directory) {
  if (::mkdir(directory.c_str(), 0755) != 0) {
    struct stat st {};
    if (::stat(directory.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) return false;
  }
  const std::string probe = directory + "/.write_probe";
  std::ofstream out(probe, std::ios::app);
  if (!out) return false;
  out << "ok\n";
  out.close();
  ::remove(probe.c_str());
  return true;
}

inline std::string choose_log_dir(const std::string& nvme_dir, const std::string& fallback_dir) {
  if (directory_writable(nvme_dir)) return nvme_dir;
  if (directory_writable(fallback_dir)) return fallback_dir;
  return fallback_dir;
}

}  // namespace drone_safety
