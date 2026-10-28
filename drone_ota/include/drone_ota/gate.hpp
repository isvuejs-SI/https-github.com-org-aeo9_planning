#pragma once

#include <string>
#include <vector>

namespace drone_ota {

struct GateConfig {
    double min_batt_pct = 0;
    double min_nvme_free_gb = 0;
    double max_jetson_c = 0;
};

struct GateState {
    bool armed = false;
    bool landed = false;
    double batt_pct = 0;
    bool charging = false;
    double nvme_free_gb = 0;
    double jetson_c = 0;
    bool failsafe_active = false;
    bool sig_ok = false;
};

GateConfig load_gate_config(const std::string& path);
GateConfig gate_config_from_yaml(const std::string& text);
std::vector<std::string> check_gate(const GateState& state, const GateConfig& config);
std::string join_csv(const std::vector<std::string>& reasons);
std::string format_gate_line(const std::string& name, const std::vector<std::string>& reasons);
std::string format_lesson_ota(const std::string& name, const std::vector<std::string>& reasons);

}  // namespace drone_ota
