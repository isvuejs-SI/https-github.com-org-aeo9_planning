#pragma once

#include <string>

namespace drone_telemetry {

struct TelemetryConfig {
    int max_bytes_per_sec = 0;
    int publish_hz = 0;
    double temp_warn_c = 0;
    double temp_fail_c = 0;
    double nvme_free_warn_pct = 0;
    double nvme_wear_warn_pct = 0;
    int max_field_len = 0;
};

TelemetryConfig load_telemetry_config(const std::string& path);
std::string jetson_level(double temp_c, double nvme_free_pct, double wear_pct, const TelemetryConfig& config);
std::string thermal_level(double temp_c, const TelemetryConfig& config);
std::string encode_summary(const std::string& mode, double battery_pct, double temp_c, double nvme_free_pct,
                           double wear_pct, bool detection, const std::string& failsafe, const TelemetryConfig& config);
bool within_budget(int nbytes, int hz, int max_bps);

}  // namespace drone_telemetry
