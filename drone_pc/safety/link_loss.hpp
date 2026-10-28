#pragma once

#include "common/params.hpp"
#include "common/units.hpp"

#include <optional>
#include <string>
#include <vector>

namespace drone_pc {

inline std::string link_loss_action(double lost_s, double rtl_after_s, double abort_after_s) {
    const int64_t lost_ms = scale_milli(lost_s);
    if (lost_ms > scale_milli(abort_after_s)) {
        return "ABORT_MISSION";
    }
    if (lost_ms > scale_milli(rtl_after_s)) {
        return "RTL";
    }
    return "CONTINUE";
}

inline std::string link_loss_action_from_limits(double lost_s, const Limits& limits) {
    return link_loss_action(lost_s, limits.link_rtl_after_s, limits.link_abort_after_s);
}

inline std::vector<std::string> requests_for_link_action(const std::string& action) {
    if (action == "RTL") return {"RTL"};
    if (action == "ABORT_MISSION") return {"LAND"};
    return {};
}

inline std::optional<std::string> mission_event_for_link(const std::string& action) {
    if (action == "RTL") return std::string("LINK_RTL");
    if (action == "ABORT_MISSION") return std::string("LINK_ABORT");
    return std::nullopt;
}

}  // namespace drone_pc
