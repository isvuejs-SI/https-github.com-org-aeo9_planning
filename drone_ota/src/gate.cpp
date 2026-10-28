#include "drone_ota/gate.hpp"

#include "drone_ota/yaml.hpp"

namespace drone_ota {
namespace {

GateConfig from_root(const Yaml& root) {
    const Yaml& gate = root.key("gate");
    GateConfig config;
    config.min_batt_pct = gate.key("min_batt_pct").number();
    config.min_nvme_free_gb = gate.key("min_nvme_free_gb").number();
    config.max_jetson_c = gate.key("max_jetson_c").number();
    return config;
}

}  // namespace

GateConfig load_gate_config(const std::string& path) {
    return from_root(load_yaml_file(path));
}

GateConfig gate_config_from_yaml(const std::string& text) {
    return from_root(parse_yaml(text));
}

std::vector<std::string> check_gate(const GateState& state, const GateConfig& config) {
    std::vector<std::string> reasons;
    if (state.armed) {
        reasons.emplace_back("armed");
    }
    if (!state.landed) {
        reasons.emplace_back("not_landed");
    }
    if (state.batt_pct < config.min_batt_pct) {
        reasons.emplace_back("battery_low");
    }
    if (!state.charging) {
        reasons.emplace_back("not_charging");
    }
    if (state.nvme_free_gb < config.min_nvme_free_gb) {
        reasons.emplace_back("nvme_full");
    }
    if (state.jetson_c > config.max_jetson_c) {
        reasons.emplace_back("too_hot");
    }
    if (state.failsafe_active) {
        reasons.emplace_back("failsafe_active");
    }
    if (!state.sig_ok) {
        reasons.emplace_back("bad_signature");
    }
    return reasons;
}

std::string join_csv(const std::vector<std::string>& reasons) {
    std::string out;
    for (std::size_t i = 0; i < reasons.size(); ++i) {
        if (i != 0) {
            out += ",";
        }
        out += reasons[i];
    }
    return out;
}

std::string format_gate_line(const std::string& name, const std::vector<std::string>& reasons) {
    if (reasons.empty()) {
        return name + ": ALLOW";
    }
    return name + ": DENY " + join_csv(reasons);
}

std::string format_lesson_ota(const std::string& name, const std::vector<std::string>& reasons) {
    if (reasons.empty()) {
        return name + ": ALLOW (keep slot B for rollback)";
    }
    return name + ": DENY " + join_csv(reasons);
}

}  // namespace drone_ota
