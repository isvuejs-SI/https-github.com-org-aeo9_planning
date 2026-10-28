#pragma once
// Stow before arming. Deploy only after landing. Refuse otherwise.
#include <string>

struct Gear {
    std::string phase;
    int landed;
    int air;
    Gear() : phase("DEPLOYED"), landed(1), air(0) {}

    std::string request_stow() {
        if (air || !landed) return "REFUSE airborne";
        phase = "STOWING";
        return "OK stowing";
    }
    std::string complete_stow() {
        if (air || !landed) return "REFUSE airborne";
        if (phase != "STOWING") return "REFUSE bad_phase";
        phase = "STOWED";
        return "OK stowed";
    }
    std::string request_deploy() {
        if (air || !landed) return "REFUSE not_landed";
        phase = "DEPLOYING";
        return "OK deploying";
    }
    std::string complete_deploy() {
        if (air || !landed) return "REFUSE not_landed";
        if (phase != "DEPLOYING") return "REFUSE bad_phase";
        phase = "DEPLOYED";
        return "OK deployed";
    }
    std::string arm() {
        if (phase != "STOWED") return "REFUSE not_stowed";
        if (air || !landed) return "REFUSE airborne";
        return "OK armed";
    }
};
