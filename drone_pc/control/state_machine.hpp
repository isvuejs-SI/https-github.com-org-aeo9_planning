#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace drone_pc {

inline const std::map<std::pair<std::string, std::string>, std::pair<std::string, std::string>>& transitions() {
    static const std::map<std::pair<std::string, std::string>, std::pair<std::string, std::string>> table = {
        {{"SLEEP_CHARGING", "START_PREFLIGHT"}, {"PREFLIGHT_GATE", "charge_ready_start_preflight"}},
        {{"SLEEP_CHARGING", "ESCAPE_TRIGGER"}, {"ESCAPE", "parked_escape"}},
        {{"PREFLIGHT_GATE", "PREFLIGHT_GO"}, {"TAKEOFF", "preflight_go"}},
        {{"PREFLIGHT_GATE", "PREFLIGHT_HOLD"}, {"PREFLIGHT_GATE", "preflight_hold_ground"}},
        {{"PREFLIGHT_GATE", "PREFLIGHT_NO_GO"}, {"SLEEP_CHARGING", "preflight_no_go"}},
        {{"PREFLIGHT_GATE", "PREFLIGHT_RTL"}, {"SLEEP_CHARGING", "preflight_rtl_on_ground"}},
        {{"PREFLIGHT_GATE", "PREFLIGHT_ABORT"}, {"SLEEP_CHARGING", "preflight_abort"}},
        {{"PREFLIGHT_GATE", "PREFLIGHT_LAND_NOW"}, {"SLEEP_CHARGING", "preflight_land_now_on_ground"}},
        {{"TAKEOFF", "TAKEOFF_COMPLETE"}, {"CRUISE", "takeoff_altitude_reached"}},
        {{"TAKEOFF", "LINK_RTL"}, {"RETURN", "link_loss_rtl"}},
        {{"TAKEOFF", "LINK_ABORT"}, {"LAND", "link_loss_abort"}},
        {{"TAKEOFF", "ENERGY_RETURN"}, {"RETURN", "energy_return"}},
        {{"TAKEOFF", "LAND_NOW"}, {"LAND", "land_now"}},
        {{"CRUISE", "REACHED_SURVEY"}, {"SURVEY", "entered_survey_area"}},
        {{"CRUISE", "ENERGY_RETURN"}, {"RETURN", "energy_return"}},
        {{"CRUISE", "LINK_RTL"}, {"RETURN", "link_loss_rtl"}},
        {{"CRUISE", "LINK_ABORT"}, {"LAND", "link_loss_abort"}},
        {{"CRUISE", "LAND_NOW"}, {"LAND", "land_now"}},
        {{"SURVEY", "SURVEY_COMPLETE"}, {"RETURN", "survey_complete"}},
        {{"SURVEY", "ENERGY_RETURN"}, {"RETURN", "energy_return"}},
        {{"SURVEY", "LINK_RTL"}, {"RETURN", "link_loss_rtl"}},
        {{"SURVEY", "LINK_ABORT"}, {"LAND", "link_loss_abort"}},
        {{"SURVEY", "LAND_NOW"}, {"LAND", "land_now"}},
        {{"RETURN", "REACHED_LAND_ZONE"}, {"LAND", "reached_landing_zone"}},
        {{"RETURN", "LINK_ABORT"}, {"LAND", "link_loss_abort"}},
        {{"RETURN", "LAND_NOW"}, {"LAND", "land_now"}},
        {{"LAND", "TOUCHDOWN"}, {"SLEEP_CHARGING", "touchdown_sleep_charge"}},
        {{"LAND", "ESCAPE_TRIGGER"}, {"ESCAPE", "landed_escape"}},
        {{"ESCAPE", "ESCAPE_COMPLETE"}, {"LAND", "escape_reached_backup"}},
        {{"ESCAPE", "LINK_ABORT"}, {"LAND", "link_loss_abort"}},
        {{"ESCAPE", "LAND_NOW"}, {"LAND", "land_now"}},
    };
    return table;
}

inline std::string transition_line(int64_t t_ms, const std::string& previous, const std::string& next, const std::string& reason) {
    return "t=" + std::to_string(t_ms) + " kind=transition from=" + previous + " to=" + next + " reason=" + reason;
}

struct TransitionResult {
    std::string state;
    std::string reason;
    std::string line;
};

class StateMachine {
public:
    explicit StateMachine(std::string state = "SLEEP_CHARGING") : state_(std::move(state)) {}

    TransitionResult handle(const std::string& event, int64_t t_ms) {
        const std::string previous = state_;
        const auto found = transitions().find({previous, event});
        std::string next = previous;
        std::string reason = "ignored_event";
        if (found != transitions().end()) {
            next = found->second.first;
            reason = found->second.second;
        }
        TransitionResult result;
        result.state = next;
        result.reason = reason;
        result.line = transition_line(t_ms, previous, next, reason);
        logs_.push_back(result.line);
        state_ = next;
        return result;
    }

    const std::string& state() const { return state_; }

private:
    std::string state_;
    std::vector<std::string> logs_;
};

}  // namespace drone_pc
