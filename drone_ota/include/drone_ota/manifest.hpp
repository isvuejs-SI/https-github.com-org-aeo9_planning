#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace drone_ota {

struct ListedFile {
    std::string sha256;
    std::string path;
    std::int64_t size = 0;
};

struct BundleInfo {
    std::string version;
    std::string hardware;
    std::string min_compatible;
    std::string max_compatible;
    std::vector<ListedFile> files;
};

std::string canonical_manifest(const std::string& version, const std::string& hardware,
                               const std::string& min_compatible, const std::string& max_compatible,
                               std::vector<ListedFile> files);

BundleInfo parse_bundle_manifest(const std::string& text);

struct LessonEntry {
    std::string sha256;
    std::string name;
    std::string version;
};

std::vector<LessonEntry> parse_lesson_manifest(const std::string& text);

std::string verify_mission(const std::string& manifest, const std::vector<std::uint8_t>& signature,
                           const std::vector<std::uint8_t>& public_key,
                           const std::map<std::string, std::string>& files, const std::string& requested,
                           bool landed, bool armed);

std::vector<std::string> verify_bundle(const std::string& manifest, const std::vector<std::uint8_t>& signature,
                                       const std::vector<std::uint8_t>& public_key,
                                       const std::map<std::string, std::string>& files,
                                       const std::string& expected_hardware, const std::string& installed_version);

}  // namespace drone_ota
