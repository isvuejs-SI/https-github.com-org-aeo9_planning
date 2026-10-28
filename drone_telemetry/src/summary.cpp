#include "drone_telemetry/summary.hpp"

#include "drone_ota/yaml.hpp"

#include <cmath>
#include <sstream>

namespace drone_telemetry {
namespace {

std::string clip(const std::string& text, int limit) {
    if (static_cast<int>(text.size()) <= limit) {
        return text;
    }
    return text.substr(0, static_cast<std::size_t>(limit));
}

std::string battery_text(double battery_pct) {
    const auto tenths_value = std::llround(battery_pct * 10.0);
    const bool negative = tenths_value < 0;
    const auto tenths = negative ? -tenths_value : tenths_value;
    std::ostringstream out;
    if (negative) {
        out << '-';
    }
    out << (tenths / 10) << '.' << (tenths % 10);
    return out.str();
}

}  // namespace

TelemetryConfig load_telemetry_config(const std::string& path) {
    const auto data = drone_ota::load_yaml_file(path).key("telemetry");
    TelemetryConfig config;
    config.max_bytes_per_sec = static_cast<int>(data.key("max_bytes_per_sec").integer());
    config.publish_hz = static_cast<int>(data.key("publish_hz").integer());
    config.temp_warn_c = data.key("temp_warn_c").number();
    config.temp_fail_c = data.key("temp_fail_c").number();
    config.nvme_free_warn_pct = data.key("nvme_free_warn_pct").number();
    config.nvme_wear_warn_pct = data.key("nvme_wear_warn_pct").number();
    config.max_field_len = static_cast<int>(data.key("max_field_len").integer());
    return config;
}

std::string jetson_level(double temp_c, double nvme_free_pct, double wear_pct, const TelemetryConfig& config) {
    if (temp_c > config.temp_fail_c || wear_pct > config.nvme_wear_warn_pct) {
        return "FAIL";
    }
    if (temp_c > config.temp_warn_c || nvme_free_pct < config.nvme_free_warn_pct) {
        return "WARN";
    }
    return "OK";
}

std::string thermal_level(double temp_c, const TelemetryConfig& config) {
    if (temp_c > config.temp_fail_c) {
        return "HOT";
    }
    if (temp_c > config.temp_warn_c) {
        return "WARM";
    }
    return "OK";
}

std::string encode_summary(const std::string& mode, double battery_pct, double temp_c, double nvme_free_pct,
                           double wear_pct, bool detection, const std::string& failsafe, const TelemetryConfig& config) {
    const auto jetson = jetson_level(temp_c, nvme_free_pct, wear_pct, config);
    const auto thermal = thermal_level(temp_c, config);
    std::ostringstream out;
    out << "mode=" << clip(mode, config.max_field_len) << " battery_pct=" << battery_text(battery_pct)
        << " jetson=" << jetson << " thermal=" << thermal << " detection=" << (detection ? 1 : 0)
        << " failsafe=" << clip(failsafe, config.max_field_len) << "\n";
    return out.str();
}

bool within_budget(int nbytes, int hz, int max_bps) {
    return nbytes * hz < max_bps;
}

}  // namespace drone_telemetry
