#include "drone_ota/slots.hpp"

#include "drone_ota/gate.hpp"

namespace drone_ota {
namespace {

std::string idle_slot(const SlotState& state) {
    return state.active == "A" ? "B" : "A";
}

}  // namespace

std::string install_idle(SlotState& state, const std::vector<std::string>& reasons) {
    if (!reasons.empty()) {
        return "DENY " + join_csv(reasons);
    }
    state.staged = idle_slot(state);
    state.pending.clear();
    return "ALLOW wrote_slot=" + state.staged + " keep_slot=" + state.active;
}

std::string boot_staged(SlotState& state) {
    if (state.staged.empty()) {
        return "NO_STAGED";
    }
    state.pending = state.staged;
    state.boot_failures = 0;
    return "BOOT pending=" + state.pending + " active=" + state.active;
}

std::string commit_pending(SlotState& state) {
    if (state.pending.empty()) {
        return "NO_PENDING";
    }
    state.active = state.pending;
    state.pending.clear();
    state.staged.clear();
    state.boot_failures = 0;
    return "COMMIT active=" + state.active;
}

std::string rollback_pending(SlotState& state) {
    state.pending.clear();
    state.staged.clear();
    state.boot_failures = 0;
    return "ROLLBACK active=" + state.active;
}

std::string note_health(SlotState& state, bool ok, int rollback_threshold) {
    if (ok) {
        state.boot_failures = 0;
        return "HEALTH_PASS";
    }
    state.boot_failures += 1;
    if (state.boot_failures >= rollback_threshold) {
        const int count = state.boot_failures;
        const std::string active = state.active;
        rollback_pending(state);
        return "HEALTH_FAIL boot_loop=" + std::to_string(count) + " action=ROLLBACK active=" + active;
    }
    return "HEALTH_FAIL boot_loop=" + std::to_string(state.boot_failures) + " action=RETRY pending=" + state.pending +
           " active=" + state.active;
}

}  // namespace drone_ota
