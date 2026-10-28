#include "common/json.hpp"
#include "common/params.hpp"
#include "control/px4_interface.hpp"
#include "control/state_machine.hpp"
#include "planning/energy.hpp"
#include "planning/escape.hpp"
#include "planning/global_planner.hpp"
#include "planning/local_planner.hpp"
#include "planning/occupancy.hpp"
#include "safety/alerts.hpp"
#include "safety/link_loss.hpp"
#include "safety/preflight.hpp"

#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

int g_failures = 0;

struct CaseFail : std::runtime_error {
    using std::runtime_error::runtime_error;
};

void check(bool condition, const std::string& message) {
    if (!condition) {
        throw CaseFail(message);
    }
}

std::string read_file(const std::string& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot read " + path);
    }
    std::stringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

bool same_num(double actual, double expected) {
    return actual == expected || std::fabs(actual - expected) <= 1e-6;
}

std::vector<double> vec3(const drone_pc::Json& value) {
    return {value.arr.at(0).num, value.arr.at(1).num, value.arr.at(2).num};
}

std::vector<int> size3(const drone_pc::Json& value) {
    return {static_cast<int>(value.arr.at(0).integer()), static_cast<int>(value.arr.at(1).integer()),
            static_cast<int>(value.arr.at(2).integer())};
}

std::vector<std::pair<double, double>> read_poly(const drone_pc::Json& value) {
    std::vector<std::pair<double, double>> polygon;
    for (const auto& point : value.arr) {
        polygon.emplace_back(point.arr.at(0).num, point.arr.at(1).num);
    }
    return polygon;
}

std::vector<std::vector<std::pair<double, double>>> read_nofly(const drone_pc::Json& value) {
    std::vector<std::vector<std::pair<double, double>>> zones;
    for (const auto& zone : value.arr) {
        zones.push_back(read_poly(zone));
    }
    return zones;
}

std::vector<std::vector<double>> read_points(const drone_pc::Json& value) {
    std::vector<std::vector<double>> points;
    for (const auto& point : value.arr) {
        points.push_back({point.arr.at(0).num, point.arr.at(1).num, point.arr.at(2).num});
    }
    return points;
}

std::vector<std::vector<int>> read_rows(const drone_pc::Json& value) {
    std::vector<std::vector<int>> rows;
    for (const auto& row : value.arr) {
        std::vector<int> item;
        for (const auto& number : row.arr) {
            item.push_back(static_cast<int>(number.integer()));
        }
        rows.push_back(item);
    }
    return rows;
}

std::vector<int> read_cell(const drone_pc::Json& value) {
    return {static_cast<int>(value.arr.at(0).integer()), static_cast<int>(value.arr.at(1).integer()),
            static_cast<int>(value.arr.at(2).integer())};
}

drone_pc::Limits limits_for(const drone_pc::Json& test_case, const std::string& root) {
    drone_pc::Limits limits = drone_pc::load_limits(root + "/config/limits.yaml");
    if (test_case.has("override")) {
        for (const auto& item : test_case.at("override").obj) {
            check(drone_pc::set_limit_field(limits, item.first, item.second.num), "unknown override " + item.first);
        }
    }
    return limits;
}

drone_pc::OccupancyGrid grid_for(const drone_pc::Json& test_case) {
    return drone_pc::OccupancyGrid(vec3(test_case.at("origin_m")), test_case.at("resolution_m").num, size3(test_case.at("size")),
                                    test_case.at("inflation_m").num);
}

void expect_strings(const std::vector<std::string>& actual, const drone_pc::Json& expected) {
    check(actual.size() == expected.arr.size(), "string list length");
    for (size_t index = 0; index < actual.size(); ++index) {
        check(actual[index] == expected.arr[index].str, "string list item " + actual[index]);
    }
}

void expect_optional_string(const std::optional<std::string>& actual, const drone_pc::Json& expected) {
    if (expected.type == drone_pc::Json::NUL) {
        check(!actual.has_value(), "expected null event");
    } else {
        check(actual.has_value() && *actual == expected.str, "event mismatch");
    }
}

bool scalar_matches(const drone_pc::Scalar& actual, const drone_pc::Json& expected) {
    if (expected.type == drone_pc::Json::BOOL) {
        return actual.kind == drone_pc::Scalar::BOOL && actual.b == expected.b;
    }
    if (expected.type == drone_pc::Json::STR) {
        return actual.kind == drone_pc::Scalar::STR && actual.s == expected.str;
    }
    if (expected.type == drone_pc::Json::NUM && expected.is_int) {
        if (actual.kind == drone_pc::Scalar::INT) {
            return actual.i == expected.inum;
        }
        if (actual.kind == drone_pc::Scalar::NUM) {
            return actual.n == static_cast<double>(expected.inum);
        }
        return false;
    }
    if (expected.type == drone_pc::Json::NUM) {
        const double value = actual.kind == drone_pc::Scalar::INT ? static_cast<double>(actual.i) : actual.n;
        return same_num(value, expected.num);
    }
    return false;
}

void run_case(const drone_pc::Json& test_case, const std::string& root) {
    const std::string& op = test_case.at("op").str;
    if (op == "load_limits") {
        const auto actual = drone_pc::scalar_map(drone_pc::load_limits(root + "/config/limits.yaml"));
        const auto& expect = test_case.at("expect").obj;
        check(actual.size() == expect.size(), "scalar map size");
        for (const auto& item : expect) {
            const auto found = actual.find(item.first);
            check(found != actual.end(), "missing scalar " + item.first);
            check(scalar_matches(found->second, item.second), "scalar mismatch " + item.first);
        }
        return;
    }
    if (op == "worst_verdict") {
        for (const auto& sample : test_case.at("samples").arr) {
            std::vector<std::string> names;
            for (const auto& name : sample.at("names").arr) {
                names.push_back(name.str);
            }
            check(drone_pc::worst_verdict(names) == sample.at("expect").str, "worst verdict");
        }
        return;
    }
    if (op == "preflight") {
        const auto limits = limits_for(test_case, root);
        const auto result = drone_pc::evaluate_preflight(
            limits, test_case.at("wind_mps").num, test_case.at("cell_temp_c").num, test_case.at("soc").num,
            test_case.at("x_m").num, test_case.at("y_m").num, test_case.at("alt_m").num, test_case.at("remote_id").str,
            test_case.at("distance_km").num, test_case.at("mission_wh").num, test_case.at("operator_abort").b);
        check(result.verdict == test_case.at("expect_verdict").str, "verdict " + result.verdict);
        check(result.reason == test_case.at("expect_reason").str, "reason " + result.reason);
        if (test_case.has("expect_checks")) {
            for (const auto& item : test_case.at("expect_checks").obj) {
                check(result.checks.at(item.first) == item.second.str, "check " + item.first);
            }
        }
        return;
    }
    if (op == "return_reserve") {
        const int64_t got = drone_pc::return_reserve_micro(
            test_case.at("distance_km").num, test_case.at("wh_per_km").num, test_case.at("distance_factor").num,
            test_case.at("pack_wh").num, test_case.at("pack_fraction").num);
        check(got == test_case.at("expect_micro_wh").integer(), "reserve");
        return;
    }
    if (op == "return_reserve_from_limits") {
        const auto limits = limits_for(test_case, root);
        const int64_t got = drone_pc::return_reserve_micro(
            test_case.at("distance_km").num, limits.wh_per_km, limits.return_distance_factor, limits.pack_capacity_wh,
            limits.pack_reserve_fraction);
        check(got == test_case.at("expect_micro_wh").integer(), "reserve from limits");
        return;
    }
    if (op == "overnight") {
        const auto got = drone_pc::overnight_decision(
            test_case.at("soc").num, test_case.at("pack_wh").num, test_case.at("solar_wh").num,
            test_case.at("distance_km").num, test_case.at("mission_wh").num, test_case.at("wh_per_km").num,
            test_case.at("distance_factor").num, test_case.at("pack_fraction").num, test_case.at("airborne").b);
        check(got == test_case.at("expect").str, got);
        return;
    }
    if (op == "backup") {
        std::vector<drone_pc::BackupCandidate> candidates;
        for (const auto& item : test_case.at("candidates").arr) {
            drone_pc::BackupCandidate candidate;
            candidate.id = static_cast<int>(item.at("id").integer());
            candidate.x = item.at("x").num;
            candidate.y = item.at("y").num;
            candidate.z = item.at("z").num;
            candidates.push_back(candidate);
        }
        std::vector<int> blocked;
        for (const auto& item : test_case.at("blocked_ids").arr) {
            blocked.push_back(static_cast<int>(item.integer()));
        }
        const auto got = drone_pc::plan_backup(
            {test_case.at("origin_m").arr.at(0).num, test_case.at("origin_m").arr.at(1).num}, candidates, blocked,
            read_poly(test_case.at("geofence")), read_nofly(test_case.at("no_fly")), test_case.at("soc").num,
            test_case.at("pack_wh").num, test_case.at("wh_per_km").num, test_case.at("distance_factor").num,
            test_case.at("pack_fraction").num);
        check(got.has_value(), "backup missing");
        const auto& expect = test_case.at("expect");
        check(got->id == expect.at("id").integer(), "backup id");
        check(got->x_mm == expect.at("x_mm").integer(), "backup x");
        check(got->y_mm == expect.at("y_mm").integer(), "backup y");
        check(got->z_mm == expect.at("z_mm").integer(), "backup z");
        check(got->distance_mm == expect.at("distance_mm").integer(), "backup distance");
        check(got->reserve_micro_wh == expect.at("reserve_micro_wh").integer(), "backup reserve");
        check(got->energy_shortfall == expect.at("energy_shortfall").b, "backup shortfall");
        return;
    }
    if (op == "astar") {
        auto grid = grid_for(test_case);
        grid.insert_points_m(read_points(test_case.at("points_m")));
        const auto path = drone_pc::plan_path(
            grid, read_cell(test_case.at("start")), read_cell(test_case.at("goal")), read_poly(test_case.at("geofence")),
            read_nofly(test_case.at("no_fly")), test_case.at("max_alt_m").num);
        const auto expect = read_rows(test_case.at("expect_path"));
        check(path == expect, "path mismatch");
        return;
    }
    if (op == "local") {
        auto grid = grid_for(test_case);
        grid.insert_points_m(read_points(test_case.at("points_m")));
        const auto got = drone_pc::plan_local(
            grid, vec3(test_case.at("position_m")), read_rows(test_case.at("path")), vec3(test_case.at("home_m")),
            test_case.at("mode").str, limits_for(test_case, root), read_poly(test_case.at("geofence")),
            read_nofly(test_case.at("no_fly")), static_cast<int>(test_case.at("lookahead_cells").integer()),
            test_case.at("vlos_max_m").num);
        const auto& expect = test_case.at("expect");
        check(got.x_mm == expect.at("x_mm").integer(), "local x");
        check(got.y_mm == expect.at("y_mm").integer(), "local y");
        check(got.z_mm == expect.at("z_mm").integer(), "local z");
        check(got.speed_mm_s == expect.at("speed_mm_s").integer(), "local speed");
        check(got.vlos_limited == expect.at("vlos_limited").b, "vlos");
        check(got.altitude_clamped == expect.at("altitude_clamped").b, "altitude");
        check(got.avoiding == expect.at("avoiding").b, "avoiding");
        return;
    }
    if (op == "cell_of") {
        auto grid = grid_for(test_case);
        int ix = 0;
        int iy = 0;
        int iz = 0;
        const auto& point = test_case.at("point_m");
        check(grid.cell_of_m(point.arr.at(0).num, point.arr.at(1).num, point.arr.at(2).num, ix, iy, iz), "cell missing");
        const auto& expect = test_case.at("expect");
        check(ix == expect.arr.at(0).integer() && iy == expect.arr.at(1).integer() && iz == expect.arr.at(2).integer(), "cell");
        return;
    }
    if (op == "pointcloud") {
        std::vector<uint8_t> data;
        for (const auto& byte : test_case.at("data").arr) {
            data.push_back(static_cast<uint8_t>(byte.integer()));
        }
        const auto points = drone_pc::decode_pointcloud2_xyz(
            data, static_cast<int>(test_case.at("point_step").integer()), static_cast<int>(test_case.at("row_step").integer()),
            static_cast<int>(test_case.at("width").integer()), static_cast<int>(test_case.at("height").integer()),
            static_cast<int>(test_case.at("x_offset").integer()), static_cast<int>(test_case.at("y_offset").integer()),
            static_cast<int>(test_case.at("z_offset").integer()));
        const auto& expect = test_case.at("expect_points");
        check(points.size() == expect.arr.size(), "point count");
        for (size_t index = 0; index < points.size(); ++index) {
            check(same_num(points[index][0], expect.arr[index].arr.at(0).num), "px");
            check(same_num(points[index][1], expect.arr[index].arr.at(1).num), "py");
            check(same_num(points[index][2], expect.arr[index].arr.at(2).num), "pz");
        }
        return;
    }
    if (op == "thermal") {
        auto grid = grid_for(test_case);
        grid.insert_points_m(read_points(test_case.at("static_points_m")));
        drone_pc::apply_thermal(grid, drone_pc::parse_thermal_json(test_case.at("message").str), test_case.at("inflation_m").num,
                                static_cast<int>(test_case.at("steps").integer()), test_case.at("dt_s").num);
        check(grid.occupied() == read_rows(test_case.at("expect_occupied")), "occupied");
        grid.clear_value(drone_pc::kDynamic);
        check(grid.occupied() == read_rows(test_case.at("expect_after_clear_dynamic")), "cleared");
        return;
    }
    if (op == "escape") {
        std::vector<drone_pc::Detection> detections;
        for (const auto& item : test_case.at("detections").arr) {
            drone_pc::Detection detection;
            detection.kind = item.at("class").str;
            detection.x = item.at("x").num;
            detection.y = item.at("y").num;
            detection.z = item.at("z").num;
            detections.push_back(detection);
        }
        std::optional<std::vector<double>> backup;
        if (test_case.at("backup_m").type != drone_pc::Json::NUL) {
            backup = vec3(test_case.at("backup_m"));
        }
        const auto result = drone_pc::evaluate_escape(
            vec3(test_case.at("drone_m")), detections, test_case.at("soc").num, test_case.at("cover_open").b,
            test_case.at("wind_mps").num, limits_for(test_case, root), backup);
        check(result.verdict == test_case.at("expect_verdict").str, result.verdict);
        check(result.action == test_case.at("expect_action").str, result.action);
        expect_strings(result.requests, test_case.at("expect_requests"));
        expect_optional_string(result.mission_event, test_case.at("expect_event"));
        if (test_case.at("expect_setpoint").type == drone_pc::Json::NUL) {
            check(!result.setpoint.has_value(), "setpoint present");
        } else {
            check(result.setpoint.has_value(), "setpoint missing");
            check(result.setpoint->x_mm == test_case.at("expect_setpoint").at("x_mm").integer(), "sx");
            check(result.setpoint->y_mm == test_case.at("expect_setpoint").at("y_mm").integer(), "sy");
            check(result.setpoint->z_mm == test_case.at("expect_setpoint").at("z_mm").integer(), "sz");
        }
        return;
    }
    if (op == "link_loss") {
        const auto action = drone_pc::link_loss_action_from_limits(test_case.at("lost_s").num, limits_for(test_case, root));
        check(action == test_case.at("expect_action").str, action);
        expect_strings(drone_pc::requests_for_link_action(action), test_case.at("expect_requests"));
        expect_optional_string(drone_pc::mission_event_for_link(action), test_case.at("expect_event"));
        return;
    }
    if (op == "state_machine") {
        drone_pc::StateMachine machine(test_case.at("start").str);
        for (const auto& event : test_case.at("events").arr) {
            const auto result = machine.handle(event.at("event").str, event.at("t_ms").integer());
            check(result.state == event.at("expect_state").str, result.state);
            check(result.reason == event.at("expect_reason").str, result.reason);
            if (event.has("expect_line")) {
                check(result.line == event.at("expect_line").str, result.line);
            } else {
                check(result.line.find("kind=transition") != std::string::npos, "transition log");
                check(result.line.find("reason=" + event.at("expect_reason").str) != std::string::npos, "reason log");
            }
        }
        return;
    }
    if (op == "heartbeat") {
        drone_pc::OffboardLink link(test_case.at("rate_hz").num, test_case.at("timeout_s").num);
        const auto& ticks = test_case.at("ticks_ms").arr;
        const auto& expect_ticks = test_case.at("expect_ticks").arr;
        check(ticks.size() == expect_ticks.size(), "tick count");
        for (size_t index = 0; index < ticks.size(); ++index) {
            expect_strings(link.tick(ticks[index].integer()), expect_ticks[index]);
        }
        const auto& probes = test_case.at("probe_after_last_ms").arr;
        const auto& missed = test_case.at("expect_missed").arr;
        for (size_t index = 0; index < probes.size(); ++index) {
            check(link.missed(probes[index].integer()) == missed[index].b, "missed");
        }
        return;
    }
    if (op == "heartbeat_from_limits") {
        const auto limits = limits_for(test_case, root);
        drone_pc::OffboardLink link(limits.heartbeat_rate_hz, limits.heartbeat_timeout_s);
        check(link.period_ms() == test_case.at("expect_period_ms").integer(), "period");
        check(link.timeout_ms() == test_case.at("expect_timeout_ms").integer(), "timeout");
        return;
    }
    if (op == "px4_phases") {
        for (const auto& item : test_case.at("expect").obj) {
            expect_strings(drone_pc::requests_for_phase(item.first), item.second);
        }
        return;
    }
    if (op == "px4_verdict") {
        expect_strings(drone_pc::requests_for_verdict(test_case.at("verdict").str, test_case.at("airborne").b), test_case.at("expect"));
        return;
    }
    if (op == "px4_reject") {
        bool threw = false;
        try {
            drone_pc::make_request(test_case.at("kind").str);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        check(threw == test_case.at("expect_error").b, "reject");
        return;
    }
    if (op == "px4_reject_setpoint") {
        std::map<std::string, int64_t> fields;
        for (const auto& item : test_case.at("setpoint").obj) {
            fields[item.first] = item.second.type == drone_pc::Json::NUM ? item.second.integer() : 0;
        }
        bool threw = false;
        try {
            drone_pc::make_request_fields("OFFBOARD_SETPOINT", fields);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        check(threw == test_case.at("expect_error").b, "reject setpoint");
        return;
    }
    if (op == "px4_setpoint") {
        std::map<std::string, int64_t> fields;
        for (const auto& item : test_case.at("setpoint").obj) {
            fields[item.first] = item.second.integer();
        }
        const auto request = drone_pc::make_request_fields("OFFBOARD_SETPOINT", fields);
        check(request.kind == test_case.at("expect").at("kind").str, "kind");
        check(request.setpoint.has_value(), "setpoint");
        const auto& expect = test_case.at("expect").at("setpoint");
        check(request.setpoint->x_mm == expect.at("x_mm").integer(), "x");
        check(request.setpoint->y_mm == expect.at("y_mm").integer(), "y");
        check(request.setpoint->z_mm == expect.at("z_mm").integer(), "z");
        return;
    }
    if (op == "px4_miss") {
        const auto response = drone_pc::missed_heartbeat_response();
        check(response.first == test_case.at("expect").at("jetson_action").str, "jetson");
        check(response.second == test_case.at("expect").at("px4_expected").str, "px4");
        expect_strings(drone_pc::px4_owned_failsafe_params(), test_case.at("expect_params"));
        return;
    }
    if (op == "ntfy") {
        drone_pc::AlertSink sink(test_case.at("cooldown_s").num);
        for (const auto& step : test_case.at("steps").arr) {
            const auto result = sink.consider(step.at("t_ms").integer(), "k", "t", "m", test_case.at("base_url").str, test_case.at("topic").str);
            check(result.sent == step.at("expect_sent").b, "sent");
            check(result.line == step.at("expect_line").str, result.line);
            if (result.sent) {
                check(result.request.has_value(), "request");
                check(result.request->url == step.at("expect_url").str, result.request->url);
                check(result.request->topic == test_case.at("topic").str, "topic");
                check(result.request->title == "t" && result.request->message == "m", "body");
            }
        }
        const auto& verdict = test_case.at("verdict_line");
        check(sink.log_verdict(verdict.at("t_ms").integer(), verdict.at("name").str, verdict.at("reason").str) == verdict.at("expect_line").str,
              "verdict line");
        return;
    }
    if (op == "log_roundtrip") {
        std::vector<std::string> lines;
        for (const auto& line : test_case.at("lines").arr) {
            lines.push_back(line.str);
        }
        const std::string directory = root + "/" + test_case.at("directory").str;
        drone_pc::write_events(directory, lines);
        check(drone_pc::read_events(directory) == lines, "log roundtrip");
        return;
    }
    throw CaseFail("unknown op " + op);
}

}  // namespace

int main(int argc, char** argv) {
    const std::string root = argc > 1 ? argv[1] : "drone_pc";
    const auto document = drone_pc::parse_json(read_file(root + "/tests/contract_cases.json"));
    for (const auto& test_case : document.at("cases").arr) {
        const std::string id = test_case.at("id").str;
        try {
            run_case(test_case, root);
        } catch (const CaseFail& error) {
            std::cerr << id << ": " << error.what() << "\n";
            ++g_failures;
        } catch (const std::exception& error) {
            std::cerr << id << ": exception " << error.what() << "\n";
            ++g_failures;
        }
    }
    if (g_failures != 0) {
        std::cerr << g_failures << " contract case(s) failed\n";
        return 1;
    }
    std::cout << document.at("cases").arr.size() << " contract cases passed\n";
    return 0;
}
