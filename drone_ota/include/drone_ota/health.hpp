#pragma once

#include <map>
#include <string>
#include <vector>

namespace drone_ota {

struct HealthConfig {
    int boot_loop_rollback_threshold = 0;
    double max_time_offset_ms = 0;
    double node_timeout_s = 0;
    double heartbeat_timeout_s = 0;
    double sensor_timeout_s = 0;
    int health_window_s = 0;
    bool tensorrt_required = false;
    std::vector<std::string> required_nodes;
    std::vector<std::string> required_sensors;
    std::string hardware_id;
};

struct Observation {
    std::vector<std::string> nodes;
    double heartbeat_age_s = 0;
    std::map<std::string, double> sensor_age_s;
    bool nvme_writable = false;
    double time_offset_ms = 0;
    int boot_failures = 0;
};

struct Platform {
    bool nvcc = false;
    bool gpu = false;
    bool cuda_linked = false;
};

HealthConfig load_health_config(const std::string& path);
std::string tensorrt_status(const Platform& platform);
Platform detect_platform();
std::string evaluate_health(const Observation& obs, const HealthConfig& config, const Platform& platform);

}  // namespace drone_ota
