#include "drone_logging/logging.hpp"

#include "drone_ota/sha256.hpp"
#include "drone_ota/yaml.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace drone_logging {
namespace {

bool ends_with(const std::string& text, const std::string& suffix) {
    return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool starts_with(const std::string& text, const std::string& prefix) {
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

std::string lower_copy(std::string text) {
    for (char& ch : text) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return text;
}

std::string base_name(const std::string& name) {
    const auto slash = name.find_last_of("/\\");
    if (slash == std::string::npos) {
        return name;
    }
    return name.substr(slash + 1);
}

bool contains(const std::vector<std::string>& values, const std::string& needle) {
    return std::find(values.begin(), values.end(), needle) != values.end();
}

std::string quote(const std::string& text) {
    return "\"" + json_escape(text) + "\"";
}

std::string field_raw(const std::string& obj, const std::string& key) {
    const std::string token = "\"" + key + "\":";
    const auto start = obj.find(token);
    if (start == std::string::npos) {
        throw std::runtime_error("missing " + key);
    }
    std::size_t cursor = start + token.size();
    if (obj[cursor] == '"') {
        std::size_t end = cursor + 1;
        while (obj[end] != '"') {
            if (obj[end] == '\\') {
                end += 2;
            } else {
                ++end;
            }
        }
        return obj.substr(cursor + 1, end - cursor - 1);
    }
    std::size_t end = cursor;
    while (end < obj.size() && obj[end] != ',' && obj[end] != '}') {
        ++end;
    }
    return obj.substr(cursor, end - cursor);
}

std::vector<Item> parse_items(const std::string& text) {
    const auto start = text.find('[');
    const auto end = text.rfind(']');
    if (start == std::string::npos || end == std::string::npos || end < start) {
        throw std::runtime_error("bad queue state");
    }
    std::string body = text.substr(start + 1, end - start - 1);
    const auto first = body.find_first_not_of(" \n\r\t");
    if (first == std::string::npos) {
        return {};
    }
    body = body.substr(first);
    std::vector<std::string> chunks;
    std::string current;
    for (std::size_t i = 0; i < body.size(); ++i) {
        if (i + 2 < body.size() && body.compare(i, 3, "},{") == 0) {
            chunks.push_back(current);
            current.clear();
            i += 2;
            continue;
        }
        current.push_back(body[i]);
    }
    if (!current.empty()) {
        chunks.push_back(current);
    }
    std::vector<Item> items;
    for (auto piece : chunks) {
        if (!piece.empty() && piece.front() == '{') {
            piece.erase(piece.begin());
        }
        if (!piece.empty() && piece.back() == '}') {
            piece.pop_back();
        }
        piece = "{" + piece + "}";
        Item item;
        item.name = field_raw(piece, "name");
        item.category = field_raw(piece, "category");
        item.link = field_raw(piece, "link");
        item.sha256 = field_raw(piece, "sha256");
        item.priority = std::stoi(field_raw(piece, "priority"));
        item.size = std::stoll(field_raw(piece, "size"));
        item.bytes_sent = std::stoll(field_raw(piece, "bytes_sent"));
        item.retries = std::stoi(field_raw(piece, "retries"));
        item.state = field_raw(piece, "state");
        items.push_back(item);
    }
    return items;
}

}  // namespace

LogConfig load_log_config(const std::string& path) {
    const drone_ota::Yaml root = drone_ota::load_yaml_file(path);
    const auto& priorities = root.key("priorities");
    const auto& storage = root.key("storage");
    const auto& upload = root.key("upload");
    LogConfig config;
    for (const auto& entry : priorities.dict) {
        config.priorities[entry.first] = static_cast<int>(entry.second.integer());
    }
    config.landing_skip = root.key("landing_skip").strings();
    config.retained = root.key("retained_categories").strings();
    config.max_retries = static_cast<int>(upload.key("max_retries").integer());
    config.link_stable_s = static_cast<int>(upload.key("link_stable_s").integer());
    config.reserve_free_percent = static_cast<int>(storage.key("reserve_free_percent").integer());
    config.delete_when_free_below_percent = static_cast<int>(storage.key("delete_when_free_below_percent").integer());
    config.stop_nonessential_free_percent = static_cast<int>(storage.key("stop_nonessential_free_percent").integer());
    config.nvme_size_gb = static_cast<int>(storage.key("nvme_size_gb").integer());
    config.os_and_staging_gb = static_cast<int>(storage.key("os_and_staging_gb").integer());
    config.est_gb_per_flight = static_cast<int>(storage.key("est_gb_per_flight").integer());
    config.lidar_mb_s = storage.key("lidar_mb_s").number();
    config.video_mb_s = storage.key("video_mb_s").number();
    config.thermal_mb_s = storage.key("thermal_mb_s").number();
    config.other_mb_s = storage.key("other_mb_s").number();
    config.max_bag_size_mb = static_cast<int>(storage.key("max_bag_size_mb").integer());
    config.topics = root.key("topics").strings();
    return config;
}

std::string json_escape(const std::string& text) {
    std::string out;
    for (unsigned char ch : text) {
        if (ch == '"') {
            out += "\\\"";
        } else if (ch == '\\') {
            out += "\\\\";
        } else if (ch == '\n') {
            out += "\\n";
        } else if (ch == '\r') {
            out += "\\r";
        } else if (ch == '\t') {
            out += "\\t";
        } else if (ch < 0x20) {
            char buf[8];
            std::snprintf(buf, sizeof(buf), "\\u%04x", ch);
            out += buf;
        } else {
            out.push_back(static_cast<char>(ch));
        }
    }
    return out;
}

std::string format_event(const std::string& kind, std::int64_t t_mono_ns, const std::string& t_wall, const std::string& src,
                         const std::string& dst, const std::string& reason) {
    std::ostringstream out;
    out << "{\"kind\":\"" << json_escape(kind) << "\",\"t_mono_ns\":" << t_mono_ns << ",\"t_wall\":\""
        << json_escape(t_wall) << "\",\"from\":\"" << json_escape(src) << "\",\"to\":\"" << json_escape(dst)
        << "\",\"reason\":\"" << json_escape(reason) << "\"}";
    return out.str();
}

void append_event(const std::string& path, const std::string& line) {
    std::ofstream output(path, std::ios::app);
    if (!output) {
        throw std::runtime_error("cannot append " + path);
    }
    output << line;
    if (line.empty() || line.back() != '\n') {
        output << '\n';
    }
}

std::string delete_decision(const std::string& category, const std::string& state,
                            const std::vector<std::string>& retained) {
    const bool kept = contains(retained, category);
    if (kept && state != "VERIFIED") {
        return "DELETE_REFUSED unverified_flight_safety";
    }
    if (state != "VERIFIED") {
        return "DELETE_REFUSED unverified";
    }
    if (kept) {
        return "DELETE_REFUSED retained_flight_safety";
    }
    return "DELETE_OK";
}

std::string apply_delete(const std::string& path, const std::string& category, const std::string& state,
                         const std::vector<std::string>& retained) {
    const std::string decision = delete_decision(category, state, retained);
    if (decision == "DELETE_OK") {
        if (::remove(path.c_str()) != 0) {
            throw std::runtime_error("cannot delete " + path);
        }
    }
    return decision;
}

std::string classify(const std::string& name) {
    const std::string base = lower_copy(base_name(name));
    if (base == "events.jsonl") {
        return "events";
    }
    if (base == "failsafe.jsonl") {
        return "failsafe";
    }
    if (ends_with(base, ".ulg")) {
        return "pixhawk";
    }
    if (base == "summary.md" || starts_with(base, "summary.")) {
        return "summary";
    }
    if (base.find("lidar") != std::string::npos) {
        return "lidar";
    }
    if (ends_with(base, ".mp4")) {
        return "video";
    }
    if (ends_with(base, ".mcap")) {
        return "bag";
    }
    return "other";
}

std::vector<std::string> ordered_names(const std::vector<std::string>& names, const LogConfig& config, bool landing_pass) {
    std::vector<std::string> kept;
    for (const auto& name : names) {
        if (landing_pass && contains(config.landing_skip, classify(name))) {
            continue;
        }
        kept.push_back(name);
    }
    std::sort(kept.begin(), kept.end(), [&](const std::string& a, const std::string& b) {
        const int pa = config.priorities.at(classify(a));
        const int pb = config.priorities.at(classify(b));
        if (pa != pb) {
            return pa < pb;
        }
        return a < b;
    });
    return kept;
}

std::string format_queue(const std::vector<std::string>& names, const std::map<std::string, std::string>& files) {
    std::ostringstream out;
    for (std::size_t i = 0; i < names.size(); ++i) {
        const auto digest = drone_ota::sha256_hex(files.at(names[i]));
        out << "Q" << (i + 1) << " " << names[i] << " " << digest.substr(0, 12) << "\n";
    }
    return out.str();
}

UploadQueue::UploadQueue(LogConfig config) : config_(std::move(config)) {}

void UploadQueue::add(const std::string& name, const std::string& data, const std::string& link) {
    Item item;
    item.name = name;
    item.category = classify(name);
    item.link = link;
    item.sha256 = drone_ota::sha256_hex(data);
    item.priority = config_.priorities.at(item.category);
    item.size = static_cast<std::int64_t>(data.size());
    items_.push_back(item);
}

Item& UploadQueue::get(const std::string& name) {
    for (auto& item : items_) {
        if (item.name == name) {
            return item;
        }
    }
    throw std::runtime_error("missing queue item " + name);
}

std::string UploadQueue::accept_chunk(const std::string& name, std::int64_t offset, std::int64_t nbytes) {
    Item& item = get(name);
    if (offset != item.bytes_sent) {
        return "OFFSET_MISMATCH";
    }
    if (item.bytes_sent + nbytes > item.size) {
        return "OVERSHOOT";
    }
    item.bytes_sent += nbytes;
    if (item.bytes_sent == item.size) {
        item.state = "UPLOADED";
        return "COMPLETE";
    }
    item.state = "UPLOADING";
    return "RESUME";
}

std::string UploadQueue::finish(const std::string& name, const std::string& received_sha) {
    Item& item = get(name);
    if (item.state == "STUCK") {
        return "STUCK";
    }
    if (item.state == "VERIFIED") {
        return "VERIFIED";
    }
    if (received_sha == item.sha256) {
        item.state = "VERIFIED";
        item.bytes_sent = item.size;
        return "VERIFIED";
    }
    item.retries += 1;
    item.bytes_sent = 0;
    if (item.retries >= config_.max_retries) {
        item.state = "STUCK";
        return "STUCK";
    }
    item.state = "QUEUED";
    return "RETRY";
}

std::string UploadQueue::dump() const {
    std::vector<Item> ordered = items_;
    std::sort(ordered.begin(), ordered.end(), [](const Item& a, const Item& b) {
        if (a.priority != b.priority) {
            return a.priority < b.priority;
        }
        return a.name < b.name;
    });
    std::ostringstream out;
    out << "{\"items\":[";
    for (std::size_t i = 0; i < ordered.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        const auto& item = ordered[i];
        out << "{\"bytes_sent\":" << item.bytes_sent << ",\"category\":" << quote(item.category)
            << ",\"link\":" << quote(item.link) << ",\"name\":" << quote(item.name) << ",\"priority\":" << item.priority
            << ",\"retries\":" << item.retries << ",\"sha256\":" << quote(item.sha256) << ",\"size\":" << item.size
            << ",\"state\":" << quote(item.state) << "}";
    }
    out << "]}\n";
    return out.str();
}

void UploadQueue::save(const std::string& path) const {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("cannot write " + path);
    }
    output << dump();
}

UploadQueue UploadQueue::load(const std::string& path, LogConfig config) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot read " + path);
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    UploadQueue queue(std::move(config));
    queue.items_ = parse_items(buffer.str());
    return queue;
}

bool link_ready(double up_for_s, bool landed, const LogConfig& config) {
    return landed && up_for_s >= static_cast<double>(config.link_stable_s);
}

std::string manifest_sha256(const std::map<std::string, std::string>& files) {
    std::ostringstream out;
    for (const auto& entry : files) {
        out << drone_ota::sha256_hex(entry.second) << "  " << entry.first << "\n";
    }
    return out.str();
}

std::string manifest_meta(const std::string& drone_id, const std::string& software, const std::string& start,
                          const std::string& end, const std::map<std::string, std::string>& files) {
    std::ostringstream entries;
    bool first = true;
    for (const auto& entry : files) {
        if (!first) {
            entries << ",";
        }
        first = false;
        entries << "{\"name\":\"" << json_escape(entry.first) << "\",\"sha256\":\"" << drone_ota::sha256_hex(entry.second)
                << "\",\"size\":" << entry.second.size() << "}";
    }
    std::ostringstream out;
    out << "{\"drone_id\":\"" << json_escape(drone_id) << "\",\"end\":\"" << json_escape(end) << "\",\"files\":["
        << entries.str() << "],\"software\":\"" << json_escape(software) << "\",\"start\":\"" << json_escape(start)
        << "\"}\n";
    return out.str();
}

bool verify_sha256_manifest(const std::string& text, const std::map<std::string, std::string>& files) {
    std::map<std::string, std::string> listed;
    std::istringstream input(text);
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        const auto gap = line.find("  ");
        if (gap == std::string::npos) {
            return false;
        }
        listed[line.substr(gap + 2)] = line.substr(0, gap);
    }
    if (listed.size() != files.size()) {
        return false;
    }
    for (const auto& entry : files) {
        const auto found = listed.find(entry.first);
        if (found == listed.end() || found->second != drone_ota::sha256_hex(entry.second)) {
            return false;
        }
    }
    return true;
}

int usable_gb(const LogConfig& config) {
    return config.nvme_size_gb * (100 - config.reserve_free_percent) / 100 - config.os_and_staging_gb;
}

int flights_capacity(const LogConfig& config) {
    return usable_gb(config) / config.est_gb_per_flight;
}

int rate_tenths(const LogConfig& config) {
    const double total = config.lidar_mb_s + config.video_mb_s + config.thermal_mb_s + config.other_mb_s;
    return static_cast<int>(std::llround(total * 10.0));
}

std::string retention_action(int free_percent, int verified_deletable, const LogConfig& config) {
    if (free_percent < config.stop_nonessential_free_percent) {
        if (verified_deletable == 0) {
            return "STOP_NONESSENTIAL_AND_ALERT";
        }
        return "DELETE_VERIFIED";
    }
    if (free_percent < config.delete_when_free_below_percent) {
        if (verified_deletable > 0) {
            return "DELETE_VERIFIED";
        }
        return "HOLD";
    }
    return "OK";
}

std::vector<std::string> record_command(const LogConfig& config) {
    const auto size_bytes = static_cast<long long>(config.max_bag_size_mb) * 1024LL * 1024LL;
    std::vector<std::string> command = {"ros2", "bag", "record", "--storage", "mcap", "--max-bag-size",
                                        std::to_string(size_bytes)};
    command.insert(command.end(), config.topics.begin(), config.topics.end());
    return command;
}

}  // namespace drone_logging
