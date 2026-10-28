#include "drone_telemetry/summary.hpp"

#include <gtest/gtest.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::string read_file(const std::string& path) {
    std::ifstream input(path);
    EXPECT_TRUE(input.good()) << path;
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

std::vector<std::string> split_pipe(const std::string& line) {
    std::vector<std::string> parts;
    std::string current;
    for (char ch : line) {
        if (ch == '|') {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    parts.push_back(current);
    return parts;
}

}  // namespace

TEST(Telemetry, PacketsAndBudget) {
    const auto config = drone_telemetry::load_telemetry_config(std::string(DRONE_TELEMETRY_SOURCE_DIR) +
                                                               "/config/telemetry.yaml");
    EXPECT_EQ(config.max_bytes_per_sec, 1024);
    EXPECT_EQ(config.publish_hz, 1);
    EXPECT_DOUBLE_EQ(config.temp_warn_c, 70.0);
    EXPECT_DOUBLE_EQ(config.temp_fail_c, 80.0);
    EXPECT_DOUBLE_EQ(config.nvme_free_warn_pct, 15.0);
    EXPECT_DOUBLE_EQ(config.nvme_wear_warn_pct, 80.0);

    std::istringstream input(read_file(std::string(DRONE_TELEMETRY_SOURCE_DIR) + "/tests/fixtures/packets.txt"));
    std::string line;
    int count = 0;
    while (std::getline(input, line)) {
        if (line.empty()) {
            continue;
        }
        const auto fields = split_pipe(line);
        ASSERT_EQ(fields.size(), 8u);
        const auto packet = drone_telemetry::encode_summary(fields[0], std::stod(fields[1]), std::stod(fields[2]),
                                                            std::stod(fields[3]), std::stod(fields[4]), fields[5] == "1",
                                                            fields[6], config);
        EXPECT_EQ(packet, fields[7] + "\n");
        EXPECT_TRUE(drone_telemetry::within_budget(static_cast<int>(packet.size()), config.publish_hz,
                                                   config.max_bytes_per_sec));
        ++count;
    }
    EXPECT_EQ(count, 5);

    EXPECT_TRUE(drone_telemetry::within_budget(1023, 1, 1024));
    EXPECT_FALSE(drone_telemetry::within_budget(1024, 1, 1024));

    const std::string mode(static_cast<std::size_t>(config.max_field_len + 8), 'M');
    const std::string fail(static_cast<std::size_t>(config.max_field_len + 4), 'N');
    const auto wide = drone_telemetry::encode_summary(mode, 50, 20, 50, 1, false, fail, config);
    EXPECT_NE(wide.find("mode=" + std::string(static_cast<std::size_t>(config.max_field_len), 'M') + " "),
              std::string::npos);
    EXPECT_NE(wide.find("failsafe=" + std::string(static_cast<std::size_t>(config.max_field_len), 'N') + "\n"),
              std::string::npos);
    EXPECT_TRUE(drone_telemetry::within_budget(static_cast<int>(wide.size()), config.publish_hz, config.max_bytes_per_sec));
    EXPECT_LT(wide.size() * static_cast<std::size_t>(config.publish_hz),
              static_cast<std::size_t>(config.max_bytes_per_sec));
}
