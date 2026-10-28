#pragma once

#include "common/units.hpp"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace drone_pc {

struct NtfyRequest {
    std::string url;
    std::string topic;
    std::string title;
    std::string message;
};

inline NtfyRequest build_ntfy(const std::string& base_url, const std::string& topic, const std::string& title, const std::string& message) {
    std::string base = base_url;
    if (!base.empty() && base.back() == '/') {
        base.pop_back();
    }
    NtfyRequest request;
    request.url = base + "/" + topic;
    request.topic = topic;
    request.title = title;
    request.message = message;
    return request;
}

struct AlertStep {
    bool sent = false;
    std::string line;
    std::optional<NtfyRequest> request;
};

class AlertSink {
public:
    explicit AlertSink(double cooldown_s) : cooldown_ms_(scale_milli(cooldown_s)) {}

    AlertStep consider(int64_t now_ms, const std::string& key, const std::string& title, const std::string& message, const std::string& base_url, const std::string& topic) {
        AlertStep step;
        const auto found = last_.find(key);
        if (found != last_.end() && now_ms - found->second < cooldown_ms_) {
            step.sent = false;
            step.line = "t=" + std::to_string(now_ms) + " kind=alert key=" + key + " action=suppressed";
            lines_.push_back(step.line);
            return step;
        }
        last_[key] = now_ms;
        step.sent = true;
        step.request = build_ntfy(base_url, topic, title, message);
        step.line = "t=" + std::to_string(now_ms) + " kind=alert key=" + key + " action=send";
        lines_.push_back(step.line);
        return step;
    }

    std::string log_verdict(int64_t now_ms, const std::string& name, const std::string& reason) {
        const std::string line = "t=" + std::to_string(now_ms) + " kind=verdict name=" + name + " reason=" + reason;
        lines_.push_back(line);
        return line;
    }

private:
    int64_t cooldown_ms_ = 0;
    std::map<std::string, int64_t> last_;
    std::vector<std::string> lines_;
};

inline std::string write_events(const std::string& directory, const std::vector<std::string>& lines) {
    std::filesystem::create_directories(directory);
    const std::string path = directory + "/events.log";
    std::ofstream output(path, std::ios::trunc);
    for (const auto& line : lines) {
        output << line << "\n";
    }
    return path;
}

inline std::vector<std::string> read_events(const std::string& directory) {
    std::ifstream input(directory + "/events.log");
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    return lines;
}

}  // namespace drone_pc
