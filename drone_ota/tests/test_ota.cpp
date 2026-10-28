#include "drone_ota/ed25519.hpp"
#include "drone_ota/gate.hpp"
#include "drone_ota/health.hpp"
#include "drone_ota/manifest.hpp"
#include "drone_ota/sha256.hpp"
#include "drone_ota/slots.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>

namespace {

std::string read_file(const std::string& path) {
    std::ifstream input(path);
    EXPECT_TRUE(input.good()) << path;
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string source_dir() { return DRONE_OTA_SOURCE_DIR; }

std::string fixture(const std::string& name) { return source_dir() + "/tests/fixtures/" + name; }

std::vector<std::string> split_char(const std::string& text, char sep) {
    std::vector<std::string> parts;
    std::string current;
    for (char ch : text) {
        if (ch == sep) {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    parts.push_back(current);
    return parts;
}

struct GateRow {
    std::string name;
    drone_ota::GateState state;
    std::vector<std::string> reasons;
};

drone_ota::GateState state_from(const std::vector<std::string>& fields, int offset) {
    drone_ota::GateState state;
    state.armed = fields[offset] == "1";
    state.landed = fields[offset + 1] == "1";
    state.batt_pct = std::stod(fields[offset + 2]);
    state.charging = fields[offset + 3] == "1";
    state.nvme_free_gb = std::stod(fields[offset + 4]);
    state.jetson_c = std::stod(fields[offset + 5]);
    state.failsafe_active = fields[offset + 6] == "1";
    state.sig_ok = fields[offset + 7] == "1";
    return state;
}

std::map<std::string, GateRow> load_gate_cases() {
    std::map<std::string, GateRow> rows;
    std::istringstream input(read_file(fixture("gate_cases.txt")));
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto fields = split_char(line, '|');
        GateRow row;
        row.name = fields[0];
        row.state = state_from(fields, 1);
        if (!fields[9].empty()) {
            row.reasons = split_char(fields[9], ',');
        }
        rows[row.name] = row;
    }
    return rows;
}

std::vector<std::string> check_row(const GateRow& row, const drone_ota::GateConfig& config) {
    return drone_ota::check_gate(row.state, config);
}

}  // namespace

TEST(Gate, FixtureCases) {
    const auto config = drone_ota::load_gate_config(source_dir() + "/config/ota_gate.yaml");
    EXPECT_DOUBLE_EQ(config.min_batt_pct, 60.0);
    EXPECT_DOUBLE_EQ(config.min_nvme_free_gb, 20.0);
    EXPECT_DOUBLE_EQ(config.max_jetson_c, 70.0);
    const auto rows = load_gate_cases();
    ASSERT_TRUE(rows.count("allow"));
    ASSERT_TRUE(rows.count("in_flight"));
    ASSERT_TRUE(rows.count("low_battery"));
    ASSERT_TRUE(rows.count("bad_sig_and_disk"));
    ASSERT_TRUE(rows.count("failsafe_blocks"));
    for (const auto& entry : rows) {
        EXPECT_EQ(check_row(entry.second, config), entry.second.reasons) << entry.first;
    }
}

TEST(Gate, Allow) {
    const auto config = drone_ota::load_gate_config(source_dir() + "/config/ota_gate.yaml");
    EXPECT_TRUE(check_row(load_gate_cases().at("allow"), config).empty());
}

TEST(Gate, InFlight) {
    const auto config = drone_ota::load_gate_config(source_dir() + "/config/ota_gate.yaml");
    const auto reasons = check_row(load_gate_cases().at("in_flight"), config);
    EXPECT_EQ(reasons, (std::vector<std::string>{"armed", "not_landed"}));
}

TEST(Gate, LowBattery) {
    const auto config = drone_ota::load_gate_config(source_dir() + "/config/ota_gate.yaml");
    EXPECT_EQ(check_row(load_gate_cases().at("low_battery"), config), std::vector<std::string>{"battery_low"});
}

TEST(Gate, BadSigAndDisk) {
    const auto config = drone_ota::load_gate_config(source_dir() + "/config/ota_gate.yaml");
    EXPECT_EQ(check_row(load_gate_cases().at("bad_sig_and_disk"), config),
              (std::vector<std::string>{"nvme_full", "bad_signature"}));
}

TEST(Gate, FailsafeBlocks) {
    const auto config = drone_ota::load_gate_config(source_dir() + "/config/ota_gate.yaml");
    EXPECT_EQ(check_row(load_gate_cases().at("failsafe_blocks"), config), std::vector<std::string>{"failsafe_active"});
}

TEST(Gate, DesignDocStdout) {
    const auto config = drone_ota::load_gate_config(source_dir() + "/config/ota_gate.yaml");
    const auto rows = load_gate_cases();
    std::istringstream input(read_file(fixture("gate_stdout.txt")));
    std::string line;
    int count = 0;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }
        const auto name = line.substr(0, line.find(": "));
        EXPECT_EQ(drone_ota::format_gate_line(name, check_row(rows.at(name), config)), line);
        ++count;
    }
    EXPECT_EQ(count, 5);
}

TEST(Gate, LessonOtaStdout) {
    const auto config = drone_ota::load_gate_config(source_dir() + "/config/ota_gate.yaml");
    std::map<std::string, drone_ota::GateState> states;
    std::istringstream cases(read_file(fixture("lesson_ota.txt")));
    std::string line;
    while (std::getline(cases, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto fields = split_char(line, '|');
        states[fields[0]] = state_from(fields, 1);
    }
    std::istringstream stdout_file(read_file(fixture("lesson_ota_stdout.txt")));
    int count = 0;
    while (std::getline(stdout_file, line)) {
        const auto name = line.substr(0, line.find(": "));
        const auto reasons = drone_ota::check_gate(states.at(name), config);
        EXPECT_EQ(drone_ota::format_lesson_ota(name, reasons), line);
        ++count;
    }
    EXPECT_EQ(count, 3);
}

TEST(Gate, ThresholdsComeFromYaml) {
    const auto shipped = drone_ota::load_gate_config(source_dir() + "/config/ota_gate.yaml");
    const auto loose = drone_ota::gate_config_from_yaml(
        "gate:\n  min_batt_pct: 10.0\n  min_nvme_free_gb: 1.0\n  max_jetson_c: 90.0\n");
    drone_ota::GateState state;
    state.landed = true;
    state.charging = true;
    state.batt_pct = 41;
    state.nvme_free_gb = 140;
    state.jetson_c = 48;
    state.sig_ok = true;
    EXPECT_EQ(drone_ota::check_gate(state, shipped), std::vector<std::string>{"battery_low"});
    EXPECT_TRUE(drone_ota::check_gate(state, loose).empty());
}

TEST(Manifest, CanonicalBytes) {
    drone_ota::ListedFile later{std::string(64, 'a'), "b.txt", 2};
    drone_ota::ListedFile earlier{std::string(64, 'b'), "a.txt", 1};
    const auto text = drone_ota::canonical_manifest("1.3.0", "jetson-orin", "1.0.0", "1.3.0", {later, earlier});
    EXPECT_EQ(text, read_file(fixture("canonical.txt")));
}

TEST(Manifest, LessonMissionCases) {
    const auto plan = read_file(fixture("mission_v1.3.0.plan"));
    const auto tampered = read_file(fixture("mission_tampered.plan"));
    EXPECT_EQ(drone_ota::sha256_hex(plan).substr(0, 12), "9b90f8973adf");
    const std::string manifest = drone_ota::sha256_hex(plan) + " mission_v1.3.0.plan 1.3.0\n";
    const auto keys = drone_ota::generate_keypair();
    const auto sig = drone_ota::sign_message(keys.priv, reinterpret_cast<const std::uint8_t*>(manifest.data()),
                                             manifest.size());
    const std::map<std::string, std::string> files = {{"mission_v1.3.0.plan", plan}, {"mission_tampered.plan", tampered},
                                                      {"other.plan", plan}};
    EXPECT_EQ(drone_ota::verify_mission(manifest, sig, keys.pub, files, "mission_v1.3.0.plan", true, false),
              "ACCEPT ack version=1.3.0");
    EXPECT_EQ(drone_ota::verify_mission(manifest, sig, keys.pub, files, "mission_tampered.plan", true, false),
              "REJECT hash_mismatch");
    EXPECT_EQ(drone_ota::verify_mission(manifest, sig, keys.pub, files, "mission_v1.3.0.plan", false, true),
              "REJECT not_landed_or_armed");
    EXPECT_EQ(drone_ota::verify_mission(manifest, sig, keys.pub, files, "other.plan", true, false),
              "REJECT name_or_version_not_pinned");
    auto bad = sig;
    bad[0] ^= 0x01;
    EXPECT_EQ(drone_ota::verify_mission(manifest, bad, keys.pub, files, "mission_v1.3.0.plan", false, true),
              "REJECT bad_signature");
}

TEST(Manifest, BundleSignatureThenHashes) {
    const std::string body = "abc";
    drone_ota::ListedFile listed{drone_ota::sha256_hex(body), "plan.txt", static_cast<std::int64_t>(body.size())};
    const auto text = drone_ota::canonical_manifest("1.4.0", "jetson-orin", "1.0.0", "2.0.0", {listed});
    const auto keys = drone_ota::generate_keypair();
    const auto sig =
        drone_ota::sign_message(keys.priv, reinterpret_cast<const std::uint8_t*>(text.data()), text.size());
    const std::map<std::string, std::string> files = {{"plan.txt", body}};
    EXPECT_TRUE(drone_ota::verify_bundle(text, sig, keys.pub, files, "jetson-orin", "1.3.0").empty());

    auto bad = sig;
    bad[0] ^= 0x01;
    EXPECT_EQ(drone_ota::verify_bundle(text, bad, keys.pub, files, "jetson-orin", "1.3.0"),
              std::vector<std::string>{"bad_signature"});
    EXPECT_EQ(drone_ota::verify_bundle(text, sig, keys.pub, {{"plan.txt", "abX"}}, "jetson-orin", "1.3.0"),
              std::vector<std::string>{"hash_mismatch"});

    const auto old_text = drone_ota::canonical_manifest("1.2.0", "jetson-orin", "1.0.0", "2.0.0", {listed});
    const auto old_sig =
        drone_ota::sign_message(keys.priv, reinterpret_cast<const std::uint8_t*>(old_text.data()), old_text.size());
    EXPECT_EQ(drone_ota::verify_bundle(old_text, old_sig, keys.pub, files, "jetson-orin", "1.3.0"),
              std::vector<std::string>{"version_rollback"});

    const auto wrong_hw = drone_ota::canonical_manifest("1.4.0", "other-board", "1.0.0", "2.0.0", {listed});
    const auto hw_sig =
        drone_ota::sign_message(keys.priv, reinterpret_cast<const std::uint8_t*>(wrong_hw.data()), wrong_hw.size());
    EXPECT_EQ(drone_ota::verify_bundle(wrong_hw, hw_sig, keys.pub, files, "jetson-orin", "1.3.0"),
              std::vector<std::string>{"wrong_hardware"});

    const auto too_new = drone_ota::canonical_manifest("1.4.0", "jetson-orin", "1.0.0", "2.0.0", {listed});
    const auto new_sig =
        drone_ota::sign_message(keys.priv, reinterpret_cast<const std::uint8_t*>(too_new.data()), too_new.size());
    EXPECT_EQ(drone_ota::verify_bundle(too_new, new_sig, keys.pub, files, "jetson-orin", "0.9.0"),
              std::vector<std::string>{"incompatible_version"});
}

TEST(Manifest, RoundTripSignature) {
    const auto keys = drone_ota::generate_keypair();
    const std::string message = "signed manifest\n";
    const auto sig = drone_ota::sign_message(keys.priv, reinterpret_cast<const std::uint8_t*>(message.data()),
                                             message.size());
    EXPECT_TRUE(drone_ota::verify_message(keys.pub, reinterpret_cast<const std::uint8_t*>(message.data()), message.size(),
                                         sig));
    auto flipped = message;
    flipped[0] = 'S';
    EXPECT_FALSE(drone_ota::verify_message(keys.pub, reinterpret_cast<const std::uint8_t*>(flipped.data()),
                                           flipped.size(), sig));
}

TEST(Slots, TraceAndBootLoop) {
    const auto health = drone_ota::load_health_config(source_dir() + "/config/ota_gate.yaml");
    ASSERT_EQ(health.boot_loop_rollback_threshold, 3);
    std::map<std::string, std::vector<std::string>> groups;
    std::istringstream input(read_file(fixture("slot_trace.txt")));
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto gap = line.find(' ');
        groups[line.substr(0, gap)].push_back(line.substr(gap + 1));
    }

    drone_ota::SlotState rollback;
    std::vector<std::string> got = {drone_ota::install_idle(rollback, {}), drone_ota::boot_staged(rollback)};
    got.push_back(drone_ota::note_health(rollback, false, health.boot_loop_rollback_threshold));
    got.push_back(drone_ota::note_health(rollback, false, health.boot_loop_rollback_threshold));
    got.push_back(drone_ota::note_health(rollback, false, health.boot_loop_rollback_threshold));
    EXPECT_EQ(got, groups["rollback"]);
    EXPECT_EQ(rollback.active, "A");
    EXPECT_TRUE(rollback.pending.empty());
    EXPECT_EQ(rollback.boot_failures, 0);

    drone_ota::SlotState denied;
    EXPECT_EQ(drone_ota::install_idle(denied, {"battery_low"}), groups["deny"][0]);
    EXPECT_EQ(denied.active, "A");
    EXPECT_TRUE(denied.staged.empty());

    drone_ota::SlotState commit;
    std::vector<std::string> committed = {drone_ota::install_idle(commit, {}), drone_ota::boot_staged(commit),
                                          drone_ota::note_health(commit, true, health.boot_loop_rollback_threshold),
                                          drone_ota::commit_pending(commit), drone_ota::install_idle(commit, {})};
    EXPECT_EQ(committed, groups["commit"]);
    EXPECT_EQ(commit.active, "B");

    drone_ota::SlotState early;
    EXPECT_EQ(drone_ota::install_idle(early, {}), "ALLOW wrote_slot=B keep_slot=A");
    EXPECT_EQ(drone_ota::boot_staged(early), "BOOT pending=B active=A");
    EXPECT_EQ(drone_ota::note_health(early, false, 2), "HEALTH_FAIL boot_loop=1 action=RETRY pending=B active=A");
    EXPECT_EQ(drone_ota::note_health(early, false, 2), "HEALTH_FAIL boot_loop=2 action=ROLLBACK active=A");
}

TEST(Health, Reports) {
    const auto config = drone_ota::load_health_config(source_dir() + "/config/ota_gate.yaml");
    EXPECT_EQ(config.boot_loop_rollback_threshold, 3);
    EXPECT_DOUBLE_EQ(config.max_time_offset_ms, 50.0);
    EXPECT_EQ(config.health_window_s, 300);
    EXPECT_FALSE(config.tensorrt_required);
    EXPECT_EQ(config.hardware_id, "jetson-orin");

    std::map<std::string, std::string> reports;
    std::string name;
    std::string body;
    std::istringstream input(read_file(fixture("health_reports.txt")));
    std::string line;
    auto flush = [&]() {
        if (!name.empty()) {
            reports[name] = body;
        }
    };
    while (std::getline(input, line)) {
        if (line.rfind("=== ", 0) == 0) {
            flush();
            name = line.substr(4);
            body.clear();
        } else {
            body += line + "\n";
        }
    }
    flush();

    drone_ota::Observation base;
    base.nodes = {"failsafe", "logger", "telemetry"};
    base.heartbeat_age_s = 1;
    base.sensor_age_s = {{"camera", 1}, {"mid360", 1}};
    base.nvme_writable = true;
    base.time_offset_ms = 0;
    base.boot_failures = 0;
    drone_ota::Platform none;
    EXPECT_EQ(drone_ota::evaluate_health(base, config, none), reports["all_ok"]);

    auto missing = base;
    missing.nodes = {"failsafe", "telemetry"};
    EXPECT_EQ(drone_ota::evaluate_health(missing, config, none), reports["missing_logger"]);

    auto disk = base;
    disk.nvme_writable = false;
    disk.boot_failures = 2;
    EXPECT_EQ(drone_ota::evaluate_health(disk, config, none), reports["rollback_nvme"]);

    auto timed = base;
    timed.time_offset_ms = 51;
    EXPECT_EQ(drone_ota::evaluate_health(timed, config, none), reports["time_fail"]);

    auto heartbeat = base;
    heartbeat.heartbeat_age_s = 4;
    EXPECT_EQ(drone_ota::evaluate_health(heartbeat, config, none), reports["heartbeat_fail"]);

    auto sensor = base;
    sensor.sensor_age_s["camera"] = 4;
    EXPECT_EQ(drone_ota::evaluate_health(sensor, config, none), reports["sensor_fail"]);

    auto both = base;
    both.sensor_age_s = {{"camera", 9}};
    EXPECT_EQ(drone_ota::evaluate_health(both, config, none), reports["both_sensors"]);

    drone_ota::Platform cuda;
    cuda.nvcc = true;
    cuda.gpu = true;
    EXPECT_EQ(drone_ota::evaluate_health(base, config, cuda), reports["cuda_unlinked"]);

    auto required = config;
    required.tensorrt_required = true;
    EXPECT_EQ(drone_ota::evaluate_health(base, required, none), reports["tensorrt_required"]);
}

TEST(Health, HostCudaIsStubbed) {
    const auto platform = drone_ota::detect_platform();
    EXPECT_FALSE(platform.cuda_linked);
    const auto status = drone_ota::tensorrt_status(platform);
    EXPECT_EQ(status.rfind("SKIP", 0), 0u);
    EXPECT_TRUE(status.find("no_nvcc_or_gpu") != std::string::npos ||
                status.find("cuda_runtime_not_linked") != std::string::npos);
    EXPECT_NE(status, "PASS");
}

TEST(Repo, NoPrivateKeyMaterial) {
    const std::string marker = std::string("BEGIN ") + "PRIVATE KEY";
    const auto root = std::filesystem::path(source_dir()).parent_path();
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto path = entry.path().string();
        if (path.find("/build/") != std::string::npos || path.find("/.git/") != std::string::npos ||
            path.find("/__pycache__/") != std::string::npos) {
            continue;
        }
        const auto name = entry.path().filename().string();
        if (name.size() >= 4 && (name.substr(name.size() - 4) == ".pyc" || name.substr(name.size() - 2) == ".o" ||
                                 name.substr(name.size() - 2) == ".a")) {
            continue;
        }
        EXPECT_EQ(name.size() >= 4 && name.substr(name.size() - 4) == ".pem", false) << path;
        std::ifstream input(entry.path(), std::ios::binary);
        std::ostringstream buffer;
        buffer << input.rdbuf();
        EXPECT_EQ(buffer.str().find(marker), std::string::npos) << path;
    }
}
