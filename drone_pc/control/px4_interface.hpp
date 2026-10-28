#pragma once

#include "common/units.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace drone_pc {

inline const std::vector<std::string>& allowed_requests() {
    static const std::vector<std::string> values = {"ARM", "TAKEOFF", "HOLD", "RTL", "LAND", "OFFBOARD_SETPOINT"};
    return values;
}

inline const std::vector<std::string>& px4_owned_failsafe_params() {
    static const std::vector<std::string> values = {"COM_OF_LOSS_T", "NAV_DLL_ACT", "COM_LOW_BAT_ACT", "COM_RCL_EXCEPT"};
    return values;
}

inline std::vector<std::string> requests_for_phase(const std::string& phase) {
    if (phase == "SLEEP_CHARGING" || phase == "PREFLIGHT_GATE") return {};
    if (phase == "TAKEOFF") return {"ARM", "TAKEOFF"};
    if (phase == "CRUISE" || phase == "SURVEY" || phase == "ESCAPE") return {"OFFBOARD_SETPOINT"};
    if (phase == "RETURN") return {"RTL"};
    if (phase == "LAND") return {"LAND"};
    throw std::runtime_error("unknown phase");
}

inline std::vector<std::string> requests_for_verdict(const std::string& verdict, bool airborne) {
    if (verdict == "GO" || verdict == "NO_GO") return {};
    if (verdict == "HOLD_GROUND") return airborne ? std::vector<std::string>{"HOLD"} : std::vector<std::string>{};
    if (verdict == "RTL") return {"RTL"};
    if (verdict == "ABORT_MISSION" || verdict == "LAND_NOW") return {"LAND"};
    throw std::runtime_error("unknown verdict");
}

struct Px4Setpoint {
    int64_t x_mm = 0;
    int64_t y_mm = 0;
    int64_t z_mm = 0;
};

struct Px4Request {
    std::string kind;
    std::optional<Px4Setpoint> setpoint;
};

inline bool is_allowed_request(const std::string& kind) {
    for (const auto& name : allowed_requests()) {
        if (name == kind) {
            return true;
        }
    }
    return false;
}

inline Px4Request make_request(const std::string& kind, const std::optional<Px4Setpoint>& setpoint = std::nullopt) {
    if (!is_allowed_request(kind)) {
        throw std::runtime_error("refusing request " + kind + "; PX4 owns actuators and failsafes");
    }
    Px4Request request;
    request.kind = kind;
    request.setpoint = setpoint;
    return request;
}

inline Px4Request make_request_fields(const std::string& kind, const std::map<std::string, int64_t>& fields) {
    for (const auto& field : fields) {
        if (field.first != "x_mm" && field.first != "y_mm" && field.first != "z_mm") {
            throw std::runtime_error("setpoint accepts position only");
        }
    }
    Px4Setpoint setpoint;
    if (fields.count("x_mm")) setpoint.x_mm = fields.at("x_mm");
    if (fields.count("y_mm")) setpoint.y_mm = fields.at("y_mm");
    if (fields.count("z_mm")) setpoint.z_mm = fields.at("z_mm");
    return make_request(kind, setpoint);
}

class OffboardLink {
public:
    OffboardLink(double rate_hz, double timeout_s)
        : period_ms_(div_round(1000000, scale_milli(rate_hz))), timeout_ms_(scale_milli(timeout_s)) {}

    std::vector<std::string> tick(int64_t now_ms) {
        if (!last_ms_.has_value() || now_ms - *last_ms_ >= period_ms_) {
            last_ms_ = now_ms;
            return {"OFFBOARD_SETPOINT"};
        }
        return {};
    }

    bool missed(int64_t now_ms) const {
        if (!last_ms_.has_value()) {
            return true;
        }
        return now_ms - *last_ms_ > timeout_ms_;
    }

    int64_t period_ms() const { return period_ms_; }
    int64_t timeout_ms() const { return timeout_ms_; }

private:
    int64_t period_ms_ = 0;
    int64_t timeout_ms_ = 0;
    std::optional<int64_t> last_ms_;
};

inline std::pair<std::string, std::string> missed_heartbeat_response() {
    return {"NONE", "PX4_HOLD_OR_RTL"};
}

}  // namespace drone_pc
