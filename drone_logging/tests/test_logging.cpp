#include "drone_logging/logging.hpp"

#include <gtest/gtest.h>

#include <fstream>
#include <map>
#include <sstream>

namespace {

std::string read_file(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    EXPECT_TRUE(input.good()) << path;
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::string source_dir() { return DRONE_LOGGING_SOURCE_DIR; }

std::string fixture(const std::string& name) { return source_dir() + "/tests/fixtures/" + name; }

std::string lesson(const std::string& name) { return read_file(fixture("lesson/" + name)); }

}  // namespace

TEST(Logging, EventsMatchFixtureAndAreKept) {
    const auto config = drone_logging::load_log_config(source_dir() + "/config/logging.yaml");
    const std::string path = "/tmp/drone_devops_events.jsonl";
    ::remove(path.c_str());
    drone_logging::append_event(path, drone_logging::format_event("mode", 10, "2026-10-24T00:00:00Z", "AUTO", "RTL",
                                                                  "low_battery"));
    drone_logging::append_event(path, drone_logging::format_event("failsafe", 11, "2026-10-24T00:00:01Z", "none", "RTL",
                                                                  "geofence"));
    drone_logging::append_event(path, drone_logging::format_event("failsafe", 12, "2026-10-24T00:00:02Z", "none", "land",
                                                                  "a \"b\" \\ c"));
    EXPECT_EQ(read_file(path), read_file(fixture("events_expected.jsonl")));
    EXPECT_EQ(drone_logging::apply_delete(path, "events", "QUEUED", config.retained),
              "DELETE_REFUSED unverified_flight_safety");
    EXPECT_EQ(read_file(path), read_file(fixture("events_expected.jsonl")));
    EXPECT_EQ(drone_logging::apply_delete(path, "events", "VERIFIED", config.retained),
              "DELETE_REFUSED retained_flight_safety");
    EXPECT_EQ(drone_logging::delete_decision("failsafe", "QUEUED", config.retained),
              "DELETE_REFUSED unverified_flight_safety");
    EXPECT_EQ(drone_logging::delete_decision("pixhawk", "UPLOADED", config.retained),
              "DELETE_REFUSED unverified_flight_safety");
    EXPECT_EQ(drone_logging::delete_decision("video", "QUEUED", config.retained), "DELETE_REFUSED unverified");

    const std::string video = "/tmp/drone_devops_video.mp4";
    std::ofstream(video) << "video";
    EXPECT_EQ(drone_logging::apply_delete(video, "video", "VERIFIED", config.retained), "DELETE_OK");
    std::ifstream gone(video);
    EXPECT_FALSE(gone.good());
}

TEST(Logging, LessonQueueAndResume) {
    const auto config = drone_logging::load_log_config(source_dir() + "/config/logging.yaml");
    ASSERT_EQ(config.max_retries, 3);
    ASSERT_EQ(config.link_stable_s, 60);
    EXPECT_TRUE(drone_logging::link_ready(60, true, config));
    EXPECT_FALSE(drone_logging::link_ready(59, true, config));
    EXPECT_FALSE(drone_logging::link_ready(60, false, config));

    const std::map<std::string, std::string> files = {
        {"events.jsonl", lesson("events.jsonl")}, {"failsafe.jsonl", lesson("failsafe.jsonl")},
        {"px4_log.ulg", lesson("px4_log.ulg")},   {"summary.md", lesson("summary.md")},
        {"cam.mp4", lesson("cam.mp4")},           {"lidar.mcap", lesson("lidar.mcap")},
        {"notes.mcap", lesson("notes.mcap")},
    };
    const std::vector<std::string> shuffled = {"cam.mp4", "summary.md", "lidar.mcap", "px4_log.ulg",
                                               "failsafe.jsonl", "events.jsonl"};
    const auto landing = drone_logging::ordered_names(shuffled, config, true);
    EXPECT_EQ(drone_logging::format_queue(landing, files), read_file(fixture("lesson_queue.txt")));

    const auto with_bag = drone_logging::ordered_names(
        {"cam.mp4", "notes.mcap", "summary.md", "lidar.mcap", "px4_log.ulg", "failsafe.jsonl", "events.jsonl"}, config,
        true);
    EXPECT_EQ(with_bag, (std::vector<std::string>{"events.jsonl", "failsafe.jsonl", "px4_log.ulg", "summary.md",
                                                  "notes.mcap"}));
    const auto fast = drone_logging::ordered_names(
        {"cam.mp4", "notes.mcap", "summary.md", "lidar.mcap", "px4_log.ulg", "failsafe.jsonl", "events.jsonl"}, config,
        false);
    EXPECT_EQ(fast, (std::vector<std::string>{"events.jsonl", "failsafe.jsonl", "px4_log.ulg", "summary.md", "notes.mcap",
                                              "cam.mp4", "lidar.mcap"}));

    drone_logging::UploadQueue queue(config);
    queue.add("events.jsonl", "hello-events", "starlink_landing");
    EXPECT_EQ(queue.accept_chunk("events.jsonl", 0, 5), "RESUME");
    EXPECT_EQ(queue.dump(), read_file(fixture("queue_state.json")));
    const std::string state_path = "/tmp/drone_devops_queue.json";
    queue.save(state_path);
    auto restored = drone_logging::UploadQueue::load(state_path, config);
    EXPECT_EQ(restored.dump(), read_file(fixture("queue_state.json")));
    EXPECT_EQ(restored.accept_chunk("events.jsonl", 0, 1), "OFFSET_MISMATCH");
    EXPECT_EQ(restored.accept_chunk("events.jsonl", 5, 7), "COMPLETE");
    EXPECT_EQ(restored.finish("events.jsonl", restored.items().at(0).sha256), "VERIFIED");

    auto tight = config;
    tight.max_retries = 1;
    drone_logging::UploadQueue once(tight);
    once.add("summary.md", "summary", "starlink_landing");
    EXPECT_EQ(once.finish("summary.md", "deadbeef"), "STUCK");

    drone_logging::UploadQueue retries(config);
    retries.add("summary.md", "summary", "starlink_landing");
    EXPECT_EQ(retries.finish("summary.md", "deadbeef"), "RETRY");
    EXPECT_EQ(retries.finish("summary.md", "deadbeef"), "RETRY");
    EXPECT_EQ(retries.finish("summary.md", "deadbeef"), "STUCK");
    EXPECT_EQ(retries.finish("summary.md", retries.items().at(0).sha256), "STUCK");
}

TEST(Logging, ManifestBudgetAndRecordCommand) {
    const auto config = drone_logging::load_log_config(source_dir() + "/config/logging.yaml");
    EXPECT_EQ(drone_logging::usable_gb(config), 700);
    EXPECT_EQ(drone_logging::flights_capacity(config), 31);
    EXPECT_EQ(drone_logging::rate_tenths(config), 121);
    EXPECT_EQ(config.reserve_free_percent, 20);
    EXPECT_EQ(config.est_gb_per_flight, 22);
    EXPECT_EQ(drone_logging::retention_action(config.stop_nonessential_free_percent, 0, config), "HOLD");
    EXPECT_EQ(drone_logging::retention_action(config.stop_nonessential_free_percent - 1, 0, config),
              "STOP_NONESSENTIAL_AND_ALERT");
    EXPECT_EQ(drone_logging::retention_action(config.stop_nonessential_free_percent - 1, 2, config), "DELETE_VERIFIED");
    EXPECT_EQ(drone_logging::retention_action(config.delete_when_free_below_percent, 3, config), "OK");
    EXPECT_EQ(drone_logging::retention_action(config.delete_when_free_below_percent - 1, 3, config), "DELETE_VERIFIED");
    EXPECT_EQ(drone_logging::retention_action(config.delete_when_free_below_percent - 1, 0, config), "HOLD");

    const std::map<std::string, std::string> files = {{"events.jsonl", lesson("events.jsonl")},
                                                      {"summary.md", lesson("summary.md")}};
    EXPECT_EQ(drone_logging::manifest_sha256(files), read_file(fixture("MANIFEST.sha256")));
    EXPECT_EQ(drone_logging::manifest_meta("aeo9", "1.3.0", "2026-10-24T01:00:00Z", "2026-10-24T02:00:00Z", files),
              read_file(fixture("MANIFEST.json")));
    EXPECT_TRUE(drone_logging::verify_sha256_manifest(drone_logging::manifest_sha256(files), files));
    auto tampered = files;
    tampered["summary.md"] += "x";
    EXPECT_FALSE(drone_logging::verify_sha256_manifest(read_file(fixture("MANIFEST.sha256")), tampered));

    const auto command = drone_logging::record_command(config);
    EXPECT_EQ(command.at(0), "ros2");
    EXPECT_EQ(command.at(4), "mcap");
    EXPECT_EQ(command.at(6), std::to_string(static_cast<long long>(config.max_bag_size_mb) * 1024LL * 1024LL));
    EXPECT_EQ(std::vector<std::string>(command.begin() + 7, command.end()), config.topics);
    const auto launch = read_file(source_dir() + "/launch/record_mcap.launch.py");
    EXPECT_NE(launch.find("mcap"), std::string::npos);
    EXPECT_NE(launch.find("generate_launch_description"), std::string::npos);
    EXPECT_NE(launch.find("not a successful bag recording"), std::string::npos);
}
