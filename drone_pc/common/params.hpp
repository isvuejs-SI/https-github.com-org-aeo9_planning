#pragma once

#include "common/yaml.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace drone_pc {

struct Scalar {
    enum Kind { BOOL, INT, NUM, STR } kind = NUM;
    bool b = false;
    int64_t i = 0;
    double n = 0.0;
    std::string s;
};

struct Limits {
    double wind_go_max_mps = 0;
    double wind_hold_max_mps = 0;
    double temp_no_go_low_c = 0;
    double temp_land_now_low_c = 0;
    double temp_hold_low_c = 0;
    double temp_hold_high_c = 0;
    double temp_land_now_high_c = 0;
    double temp_no_go_high_c = 0;
    double pack_capacity_wh = 0;
    double wh_per_km = 0;
    double return_distance_factor = 0;
    double pack_reserve_fraction = 0;
    double soc_go_min = 0;
    double soc_hold_min = 0;
    double soc_rtl_min = 0;
    double escape_min_soc = 0;
    std::vector<std::pair<double, double>> geofence;
    std::vector<std::vector<std::pair<double, double>>> no_fly;
    double alt_min_m = 0;
    double alt_max_m = 0;
    double speed_max_mps = 0;
    double speed_cruise_mps = 0;
    double speed_survey_mps = 0;
    double speed_takeoff_mps = 0;
    double speed_escape_mps = 0;
    double speed_return_mps = 0;
    double speed_land_mps = 0;
    double slow_radius_m = 0;
    int lookahead_cells = 0;
    double vlos_max_range_m = 0;
    double heartbeat_rate_hz = 0;
    double heartbeat_timeout_s = 0;
    double link_rtl_after_s = 0;
    double link_abort_after_s = 0;
    std::string ntfy_base_url;
    std::string ntfy_topic;
    double ntfy_cooldown_s = 0;
    bool ntfy_enable_send = false;
    std::string log_directory;
    double escape_watch_range_m = 0;
    double escape_deter_range_m = 0;
    double escape_escape_range_m = 0;
    double escape_prop_lockout_range_m = 0;
    double escape_climb_m = 0;
    double escape_max_wind_mps = 0;
    double grid_resolution_m = 0;
    double grid_inflation_m = 0;
    int grid_predict_steps = 0;
    double grid_predict_dt_s = 0;
    int grid_size_x = 0;
    int grid_size_y = 0;
    int grid_size_z = 0;
    double grid_origin_x_m = 0;
    double grid_origin_y_m = 0;
    double grid_origin_z_m = 0;
    std::string topic_pointcloud;
    std::string topic_thermal;
    std::string topic_link;
    std::string topic_px4_request;

    double speed_for_mode(const std::string& mode) const {
        if (mode == "takeoff") return speed_takeoff_mps;
        if (mode == "cruise") return speed_cruise_mps;
        if (mode == "survey") return speed_survey_mps;
        if (mode == "return") return speed_return_mps;
        if (mode == "escape") return speed_escape_mps;
        if (mode == "land") return speed_land_mps;
        throw std::runtime_error("unknown speed mode");
    }
};

inline std::vector<std::pair<double, double>> read_polygon(const Yaml& node) {
    std::vector<std::pair<double, double>> polygon;
    for (const auto& point : node.seq) {
        polygon.emplace_back(point.key("x").number(), point.key("y").number());
    }
    return polygon;
}

inline Limits limits_from_yaml(const Yaml& raw) {
    const Yaml& wind = raw.key("wind");
    const Yaml& temperature = raw.key("temperature");
    const Yaml& battery = raw.key("battery");
    const Yaml& geofence = raw.key("geofence");
    const Yaml& altitude = raw.key("altitude");
    const Yaml& speed = raw.key("speed");
    const Yaml& local = raw.key("local");
    const Yaml& vlos = raw.key("vlos");
    const Yaml& heartbeat = raw.key("heartbeat");
    const Yaml& link = raw.key("link");
    const Yaml& ntfy = raw.key("ntfy");
    const Yaml& logging = raw.key("logging");
    const Yaml& escape = raw.key("escape");
    const Yaml& grid = raw.key("grid");
    const Yaml& topics = raw.key("topics");
    Limits limits;
    limits.wind_go_max_mps = wind.key("go_max_mps").number();
    limits.wind_hold_max_mps = wind.key("hold_max_mps").number();
    limits.temp_no_go_low_c = temperature.key("no_go_low_c").number();
    limits.temp_land_now_low_c = temperature.key("land_now_low_c").number();
    limits.temp_hold_low_c = temperature.key("hold_low_c").number();
    limits.temp_hold_high_c = temperature.key("hold_high_c").number();
    limits.temp_land_now_high_c = temperature.key("land_now_high_c").number();
    limits.temp_no_go_high_c = temperature.key("no_go_high_c").number();
    limits.pack_capacity_wh = battery.key("pack_capacity_wh").number();
    limits.wh_per_km = battery.key("wh_per_km").number();
    limits.return_distance_factor = battery.key("return_distance_factor").number();
    limits.pack_reserve_fraction = battery.key("pack_reserve_fraction").number();
    limits.soc_go_min = battery.key("soc_go_min").number();
    limits.soc_hold_min = battery.key("soc_hold_min").number();
    limits.soc_rtl_min = battery.key("soc_rtl_min").number();
    limits.escape_min_soc = battery.key("escape_min_soc").number();
    limits.geofence = read_polygon(geofence.key("polygon"));
    for (const auto& zone : raw.key("no_fly").seq) {
        limits.no_fly.push_back(read_polygon(zone.key("polygon")));
    }
    limits.alt_min_m = altitude.key("min_m").number();
    limits.alt_max_m = altitude.key("max_m").number();
    limits.speed_max_mps = speed.key("max_mps").number();
    limits.speed_cruise_mps = speed.key("cruise_mps").number();
    limits.speed_survey_mps = speed.key("survey_mps").number();
    limits.speed_takeoff_mps = speed.key("takeoff_mps").number();
    limits.speed_escape_mps = speed.key("escape_mps").number();
    limits.speed_return_mps = speed.key("return_mps").number();
    limits.speed_land_mps = speed.key("land_mps").number();
    limits.slow_radius_m = speed.key("slow_radius_m").number();
    limits.lookahead_cells = static_cast<int>(local.key("lookahead_cells").integer());
    limits.vlos_max_range_m = vlos.key("max_range_m").number();
    limits.heartbeat_rate_hz = heartbeat.key("rate_hz").number();
    limits.heartbeat_timeout_s = heartbeat.key("timeout_s").number();
    limits.link_rtl_after_s = link.key("rtl_after_s").number();
    limits.link_abort_after_s = link.key("abort_after_s").number();
    limits.ntfy_base_url = ntfy.key("base_url").text();
    limits.ntfy_topic = ntfy.key("topic").text();
    limits.ntfy_cooldown_s = ntfy.key("cooldown_s").number();
    limits.ntfy_enable_send = ntfy.key("enable_send").boolean();
    limits.log_directory = logging.key("directory").text();
    limits.escape_watch_range_m = escape.key("watch_range_m").number();
    limits.escape_deter_range_m = escape.key("deter_range_m").number();
    limits.escape_escape_range_m = escape.key("escape_range_m").number();
    limits.escape_prop_lockout_range_m = escape.key("prop_lockout_range_m").number();
    limits.escape_climb_m = escape.key("climb_m").number();
    limits.escape_max_wind_mps = escape.key("max_wind_mps").number();
    limits.grid_resolution_m = grid.key("resolution_m").number();
    limits.grid_inflation_m = grid.key("inflation_m").number();
    limits.grid_predict_steps = static_cast<int>(grid.key("predict_steps").integer());
    limits.grid_predict_dt_s = grid.key("predict_dt_s").number();
    limits.grid_size_x = static_cast<int>(grid.key("size_x").integer());
    limits.grid_size_y = static_cast<int>(grid.key("size_y").integer());
    limits.grid_size_z = static_cast<int>(grid.key("size_z").integer());
    limits.grid_origin_x_m = grid.key("origin_x_m").number();
    limits.grid_origin_y_m = grid.key("origin_y_m").number();
    limits.grid_origin_z_m = grid.key("origin_z_m").number();
    limits.topic_pointcloud = topics.key("pointcloud").text();
    limits.topic_thermal = topics.key("thermal").text();
    limits.topic_link = topics.key("link").text();
    limits.topic_px4_request = topics.key("px4_request").text();
    return limits;
}

inline Limits load_limits(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open " + path);
    }
    std::stringstream buffer;
    buffer << input.rdbuf();
    return limits_from_yaml(parse_yaml(buffer.str()));
}

inline Scalar num_scalar(double value) {
    Scalar scalar;
    scalar.kind = Scalar::NUM;
    scalar.n = value;
    return scalar;
}

inline Scalar int_scalar(int64_t value) {
    Scalar scalar;
    scalar.kind = Scalar::INT;
    scalar.i = value;
    return scalar;
}

inline Scalar str_scalar(const std::string& value) {
    Scalar scalar;
    scalar.kind = Scalar::STR;
    scalar.s = value;
    return scalar;
}

inline Scalar bool_scalar(bool value) {
    Scalar scalar;
    scalar.kind = Scalar::BOOL;
    scalar.b = value;
    return scalar;
}

inline std::map<std::string, Scalar> scalar_map(const Limits& limits) {
    std::map<std::string, Scalar> data;
    data["wind_go_max_mps"] = num_scalar(limits.wind_go_max_mps);
    data["wind_hold_max_mps"] = num_scalar(limits.wind_hold_max_mps);
    data["temp_no_go_low_c"] = num_scalar(limits.temp_no_go_low_c);
    data["temp_land_now_low_c"] = num_scalar(limits.temp_land_now_low_c);
    data["temp_hold_low_c"] = num_scalar(limits.temp_hold_low_c);
    data["temp_hold_high_c"] = num_scalar(limits.temp_hold_high_c);
    data["temp_land_now_high_c"] = num_scalar(limits.temp_land_now_high_c);
    data["temp_no_go_high_c"] = num_scalar(limits.temp_no_go_high_c);
    data["pack_capacity_wh"] = num_scalar(limits.pack_capacity_wh);
    data["wh_per_km"] = num_scalar(limits.wh_per_km);
    data["return_distance_factor"] = num_scalar(limits.return_distance_factor);
    data["pack_reserve_fraction"] = num_scalar(limits.pack_reserve_fraction);
    data["soc_go_min"] = num_scalar(limits.soc_go_min);
    data["soc_hold_min"] = num_scalar(limits.soc_hold_min);
    data["soc_rtl_min"] = num_scalar(limits.soc_rtl_min);
    data["escape_min_soc"] = num_scalar(limits.escape_min_soc);
    data["alt_min_m"] = num_scalar(limits.alt_min_m);
    data["alt_max_m"] = num_scalar(limits.alt_max_m);
    data["speed_max_mps"] = num_scalar(limits.speed_max_mps);
    data["speed_cruise_mps"] = num_scalar(limits.speed_cruise_mps);
    data["speed_survey_mps"] = num_scalar(limits.speed_survey_mps);
    data["speed_takeoff_mps"] = num_scalar(limits.speed_takeoff_mps);
    data["speed_escape_mps"] = num_scalar(limits.speed_escape_mps);
    data["speed_return_mps"] = num_scalar(limits.speed_return_mps);
    data["speed_land_mps"] = num_scalar(limits.speed_land_mps);
    data["slow_radius_m"] = num_scalar(limits.slow_radius_m);
    data["lookahead_cells"] = int_scalar(limits.lookahead_cells);
    data["vlos_max_range_m"] = num_scalar(limits.vlos_max_range_m);
    data["heartbeat_rate_hz"] = num_scalar(limits.heartbeat_rate_hz);
    data["heartbeat_timeout_s"] = num_scalar(limits.heartbeat_timeout_s);
    data["link_rtl_after_s"] = num_scalar(limits.link_rtl_after_s);
    data["link_abort_after_s"] = num_scalar(limits.link_abort_after_s);
    data["ntfy_base_url"] = str_scalar(limits.ntfy_base_url);
    data["ntfy_topic"] = str_scalar(limits.ntfy_topic);
    data["ntfy_cooldown_s"] = num_scalar(limits.ntfy_cooldown_s);
    data["ntfy_enable_send"] = bool_scalar(limits.ntfy_enable_send);
    data["log_directory"] = str_scalar(limits.log_directory);
    data["escape_watch_range_m"] = num_scalar(limits.escape_watch_range_m);
    data["escape_deter_range_m"] = num_scalar(limits.escape_deter_range_m);
    data["escape_escape_range_m"] = num_scalar(limits.escape_escape_range_m);
    data["escape_prop_lockout_range_m"] = num_scalar(limits.escape_prop_lockout_range_m);
    data["escape_climb_m"] = num_scalar(limits.escape_climb_m);
    data["escape_max_wind_mps"] = num_scalar(limits.escape_max_wind_mps);
    data["grid_resolution_m"] = num_scalar(limits.grid_resolution_m);
    data["grid_inflation_m"] = num_scalar(limits.grid_inflation_m);
    data["grid_predict_steps"] = int_scalar(limits.grid_predict_steps);
    data["grid_predict_dt_s"] = num_scalar(limits.grid_predict_dt_s);
    data["grid_size_x"] = int_scalar(limits.grid_size_x);
    data["grid_size_y"] = int_scalar(limits.grid_size_y);
    data["grid_size_z"] = int_scalar(limits.grid_size_z);
    data["grid_origin_x_m"] = num_scalar(limits.grid_origin_x_m);
    data["grid_origin_y_m"] = num_scalar(limits.grid_origin_y_m);
    data["grid_origin_z_m"] = num_scalar(limits.grid_origin_z_m);
    data["topic_pointcloud"] = str_scalar(limits.topic_pointcloud);
    data["topic_thermal"] = str_scalar(limits.topic_thermal);
    data["topic_link"] = str_scalar(limits.topic_link);
    data["topic_px4_request"] = str_scalar(limits.topic_px4_request);
    data["geofence_points"] = int_scalar(static_cast<int64_t>(limits.geofence.size()));
    data["no_fly_zones"] = int_scalar(static_cast<int64_t>(limits.no_fly.size()));
    data["geofence_first_x"] = num_scalar(limits.geofence.at(0).first);
    data["geofence_first_y"] = num_scalar(limits.geofence.at(0).second);
    data["no_fly_first_x"] = num_scalar(limits.no_fly.at(0).at(0).first);
    data["no_fly_first_y"] = num_scalar(limits.no_fly.at(0).at(0).second);
    return data;
}

inline bool set_limit_field(Limits& limits, const std::string& key, double value) {
    if (key == "wind_go_max_mps") {
        limits.wind_go_max_mps = value;
        return true;
    }
    if (key == "escape_escape_range_m") {
        limits.escape_escape_range_m = value;
        return true;
    }
    return false;
}

}  // namespace drone_pc
