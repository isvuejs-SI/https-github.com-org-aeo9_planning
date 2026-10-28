#pragma once

#include <string>
#include <vector>

namespace drone_ota {

struct SlotState {
    std::string active = "A";
    std::string staged;
    std::string pending;
    int boot_failures = 0;
};

std::string install_idle(SlotState& state, const std::vector<std::string>& reasons);
std::string boot_staged(SlotState& state);
std::string commit_pending(SlotState& state);
std::string rollback_pending(SlotState& state);
std::string note_health(SlotState& state, bool ok, int rollback_threshold);

}  // namespace drone_ota
