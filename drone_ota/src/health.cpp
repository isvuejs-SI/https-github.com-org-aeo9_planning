#include "drone_ota/health.hpp"

#include "drone_ota/yaml.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace drone_ota {
namespace {

bool executable_on_path(const std::string& name) {
    const char* path = std::getenv("PATH");
    if (path == nullptr) {
        return false;
    }
    std::stringstream stream(path);
    std::string dir;
    while (std::getline(stream, dir, ':')) {
        if (dir.empty()) {
            continue;
        }
        std::ifstream file(dir + "/" + name);
        if (file.good()) {
            return true;
        }
    }
    return false;
}

bool device_exists(const std::string& path) {
    std::ifstream file(path);
    return file.good();
}

std::string join_plus(std::vector<std::string> names) {
    std::sort(names.begin(), names.end());
    std::string out;
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i != 0) {
            out += "+";
        }
        out += names[i];
    }
    return out;
}

}  // namespace

HealthConfig load_health_config(const std::string& path) {
    const Yaml root = load_yaml_file(path);
    const Yaml& health = root.key("health");
    HealthConfig config;
    config.boot_loop_rollback_threshold = static_cast<int>(health.key("boot_loop_rollback_threshold").integer());
    config.max_time_offset_ms = health.key("max_time_offset_ms").number();
    config.node_timeout_s = health.key("node_timeout_s").number();
    config.heartbeat_timeout_s = health.key("heartbeat_timeout_s").number();
    config.sensor_timeout_s = health.key("sensor_timeout_s").number();
    config.health_window_s = static_cast<int>(health.key("health_window_s").integer());
    config.tensorrt_required = health.key("tensorrt_required").flag();
    config.required_nodes = health.key("required_nodes").strings();
    config.required_sensors = health.key("required_sensors").strings();
    config.hardware_id = root.key("hardware").key("id").text();
    return config;
}

std::string tensorrt_status(const Platform& platform) {
    // The device image would call cudaGetDeviceCount and load the TensorRT
    // engine. This host binary is not linked to CUDA, so it never reports PASS
    // unless a caller injects cuda_linked for a unit test.
    if (!platform.nvcc || !platform.gpu) {
        return "SKIP no_nvcc_or_gpu";
    }
    if (!platform.cuda_linked) {
        return "SKIP cuda_runtime_not_linked";
    }
    return "PASS";
}

Platform detect_platform() {
    Platform platform;
    platform.nvcc = executable_on_path("nvcc");
    platform.gpu = device_exists("/dev/nvidia0") || device_exists("/dev/nvidiactl");
    platform.cuda_linked = false;
    return platform;
}

std::string evaluate_health(const Observation& obs, const HealthConfig& config, const Platform& platform) {
    std::vector<std::string> missing_nodes;
    for (const auto& name : config.required_nodes) {
        if (std::find(obs.nodes.begin(), obs.nodes.end(), name) == obs.nodes.end()) {
            missing_nodes.push_back(name);
        }
    }
    const bool heartbeat_ok = !(obs.heartbeat_age_s > config.heartbeat_timeout_s);
    std::vector<std::string> missing_sensors;
    for (const auto& name : config.required_sensors) {
        const auto found = obs.sensor_age_s.find(name);
        if (found == obs.sensor_age_s.end() || found->second > config.sensor_timeout_s) {
            missing_sensors.push_back(name);
        }
    }
    const std::string tensorrt = tensorrt_status(platform);
    const bool nvme_ok = obs.nvme_writable;
    const bool time_ok = !(obs.time_offset_ms > config.max_time_offset_ms) &&
                         !(obs.time_offset_ms < -config.max_time_offset_ms);
    const bool tensorrt_ok = tensorrt == "PASS" || (tensorrt.rfind("SKIP", 0) == 0 && !config.tensorrt_required);

    bool failed = !missing_nodes.empty() || !heartbeat_ok || !missing_sensors.empty() || !nvme_ok || !time_ok ||
                  !tensorrt_ok;
    int boot_out = obs.boot_failures;
    std::string action = "OK";
    if (!failed) {
        boot_out = 0;
        action = "OK";
    } else {
        boot_out = obs.boot_failures + 1;
        action = boot_out >= config.boot_loop_rollback_threshold ? "ROLLBACK" : "RETRY";
    }

    std::ostringstream out;
    out << "nodes=" << (missing_nodes.empty() ? "PASS" : "FAIL missing=" + join_plus(missing_nodes)) << "\n";
    out << "mavlink_heartbeat=" << (heartbeat_ok ? "PASS" : "FAIL") << "\n";
    out << "sensors=" << (missing_sensors.empty() ? "PASS" : "FAIL missing=" + join_plus(missing_sensors)) << "\n";
    out << "tensorrt=" << tensorrt << "\n";
    out << "nvme=" << (nvme_ok ? "PASS" : "FAIL") << "\n";
    out << "time_sync=" << (time_ok ? "PASS" : "FAIL") << "\n";
    out << "boot_failures=" << boot_out << "\n";
    out << "action=" << action << "\n";
    return out.str();
}

}  // namespace drone_ota
