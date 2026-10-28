#include "drone_ota/manifest.hpp"

#include "drone_ota/ed25519.hpp"
#include "drone_ota/sha256.hpp"

#include <algorithm>
#include <sstream>
#include <stdexcept>

namespace drone_ota {
namespace {

std::vector<int> version_parts(const std::string& text) {
    std::vector<int> parts;
    std::stringstream stream(text);
    std::string piece;
    while (std::getline(stream, piece, '.')) {
        if (piece.empty()) {
            throw std::runtime_error("bad version");
        }
        std::size_t used = 0;
        const int value = std::stoi(piece, &used, 10);
        if (used != piece.size()) {
            throw std::runtime_error("bad version");
        }
        parts.push_back(value);
    }
    return parts;
}

int cmp_version(const std::string& left, const std::string& right) {
    auto a = version_parts(left);
    auto b = version_parts(right);
    const std::size_t n = std::max(a.size(), b.size());
    a.resize(n, 0);
    b.resize(n, 0);
    if (a < b) {
        return -1;
    }
    if (b < a) {
        return 1;
    }
    return 0;
}

std::vector<std::string> split_ws(const std::string& line) {
    std::vector<std::string> parts;
    std::stringstream stream(line);
    std::string token;
    while (stream >> token) {
        parts.push_back(token);
    }
    return parts;
}

}  // namespace

std::string canonical_manifest(const std::string& version, const std::string& hardware,
                               const std::string& min_compatible, const std::string& max_compatible,
                               std::vector<ListedFile> files) {
    std::sort(files.begin(), files.end(), [](const ListedFile& a, const ListedFile& b) { return a.path < b.path; });
    std::ostringstream out;
    out << "DRONEOTA1\n";
    out << "version=" << version << "\n";
    out << "hardware=" << hardware << "\n";
    out << "min_compatible=" << min_compatible << "\n";
    out << "max_compatible=" << max_compatible << "\n";
    for (const auto& file : files) {
        out << file.sha256 << "  " << file.size << " " << file.path << "\n";
    }
    return out.str();
}

BundleInfo parse_bundle_manifest(const std::string& text) {
    std::istringstream input(text);
    std::string line;
    if (!std::getline(input, line) || line != "DRONEOTA1") {
        throw std::runtime_error("not a bundle manifest");
    }
    BundleInfo info;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        const auto eq = line.find('=');
        const auto gap = line.find("  ");
        if (eq != std::string::npos && (gap == std::string::npos || eq < gap)) {
            const std::string key = line.substr(0, eq);
            const std::string value = line.substr(eq + 1);
            if (key == "version") {
                info.version = value;
            } else if (key == "hardware") {
                info.hardware = value;
            } else if (key == "min_compatible") {
                info.min_compatible = value;
            } else if (key == "max_compatible") {
                info.max_compatible = value;
            } else {
                throw std::runtime_error("unknown manifest key");
            }
            continue;
        }
        const auto parts = split_ws(line);
        if (parts.size() != 3) {
            throw std::runtime_error("bad file line");
        }
        ListedFile file;
        file.sha256 = parts[0];
        file.size = std::stoll(parts[1]);
        file.path = parts[2];
        info.files.push_back(file);
    }
    return info;
}

std::vector<LessonEntry> parse_lesson_manifest(const std::string& text) {
    std::vector<LessonEntry> entries;
    std::istringstream input(text);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        const auto parts = split_ws(line);
        if (parts.size() != 3) {
            throw std::runtime_error("bad lesson manifest");
        }
        entries.push_back(LessonEntry{parts[0], parts[1], parts[2]});
    }
    return entries;
}

std::string verify_mission(const std::string& manifest, const std::vector<std::uint8_t>& signature,
                           const std::vector<std::uint8_t>& public_key,
                           const std::map<std::string, std::string>& files, const std::string& requested,
                           bool landed, bool armed) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(manifest.data());
    if (!verify_message(public_key, bytes, manifest.size(), signature)) {
        return "REJECT bad_signature";
    }
    if (armed || !landed) {
        return "REJECT not_landed_or_armed";
    }
    const auto entries = parse_lesson_manifest(manifest);
    if (entries.empty()) {
        return "REJECT hash_mismatch";
    }
    const auto& want = entries.front();
    const auto found = files.find(requested);
    if (found == files.end() || sha256_hex(found->second) != want.sha256) {
        return "REJECT hash_mismatch";
    }
    if (requested != want.name) {
        return "REJECT name_or_version_not_pinned";
    }
    return "ACCEPT ack version=" + want.version;
}

std::vector<std::string> verify_bundle(const std::string& manifest, const std::vector<std::uint8_t>& signature,
                                       const std::vector<std::uint8_t>& public_key,
                                       const std::map<std::string, std::string>& files,
                                       const std::string& expected_hardware, const std::string& installed_version) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(manifest.data());
    if (!verify_message(public_key, bytes, manifest.size(), signature)) {
        return {"bad_signature"};
    }
    const BundleInfo info = parse_bundle_manifest(manifest);
    std::vector<std::string> reasons;
    bool hash_bad = false;
    for (const auto& listed : info.files) {
        const auto found = files.find(listed.path);
        if (found == files.end() || static_cast<std::int64_t>(found->second.size()) != listed.size ||
            sha256_hex(found->second) != listed.sha256) {
            hash_bad = true;
        }
    }
    if (hash_bad) {
        reasons.emplace_back("hash_mismatch");
    }
    if (info.hardware != expected_hardware) {
        reasons.emplace_back("wrong_hardware");
    }
    if (cmp_version(installed_version, info.min_compatible) < 0 ||
        cmp_version(installed_version, info.max_compatible) > 0) {
        reasons.emplace_back("incompatible_version");
    }
    if (cmp_version(info.version, installed_version) <= 0) {
        reasons.emplace_back("version_rollback");
    }
    return reasons;
}

}  // namespace drone_ota
