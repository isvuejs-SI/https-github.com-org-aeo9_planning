#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace drone_logging {

struct LogConfig {
    std::map<std::string, int> priorities;
    std::vector<std::string> landing_skip;
    std::vector<std::string> retained;
    int max_retries = 0;
    int link_stable_s = 0;
    int reserve_free_percent = 0;
    int delete_when_free_below_percent = 0;
    int stop_nonessential_free_percent = 0;
    int nvme_size_gb = 0;
    int os_and_staging_gb = 0;
    int est_gb_per_flight = 0;
    double lidar_mb_s = 0;
    double video_mb_s = 0;
    double thermal_mb_s = 0;
    double other_mb_s = 0;
    int max_bag_size_mb = 0;
    std::vector<std::string> topics;
};

struct Item {
    std::string name;
    std::string category;
    std::string link;
    std::string sha256;
    int priority = 0;
    std::int64_t size = 0;
    std::int64_t bytes_sent = 0;
    int retries = 0;
    std::string state = "QUEUED";
};

LogConfig load_log_config(const std::string& path);
std::string json_escape(const std::string& text);
std::string format_event(const std::string& kind, std::int64_t t_mono_ns, const std::string& t_wall,
                         const std::string& src, const std::string& dst, const std::string& reason);
void append_event(const std::string& path, const std::string& line);
std::string delete_decision(const std::string& category, const std::string& state,
                            const std::vector<std::string>& retained);
std::string apply_delete(const std::string& path, const std::string& category, const std::string& state,
                         const std::vector<std::string>& retained);

std::string classify(const std::string& name);
std::vector<std::string> ordered_names(const std::vector<std::string>& names, const LogConfig& config,
                                       bool landing_pass);
std::string format_queue(const std::vector<std::string>& names, const std::map<std::string, std::string>& files);

class UploadQueue {
public:
    explicit UploadQueue(LogConfig config);
    void add(const std::string& name, const std::string& data, const std::string& link);
    std::string accept_chunk(const std::string& name, std::int64_t offset, std::int64_t nbytes);
    std::string finish(const std::string& name, const std::string& received_sha);
    std::string dump() const;
    void save(const std::string& path) const;
    static UploadQueue load(const std::string& path, LogConfig config);
    const std::vector<Item>& items() const { return items_; }

private:
    LogConfig config_;
    std::vector<Item> items_;
    Item& get(const std::string& name);
};

bool link_ready(double up_for_s, bool landed, const LogConfig& config);
std::string manifest_sha256(const std::map<std::string, std::string>& files);
std::string manifest_meta(const std::string& drone_id, const std::string& software, const std::string& start,
                          const std::string& end, const std::map<std::string, std::string>& files);
bool verify_sha256_manifest(const std::string& text, const std::map<std::string, std::string>& files);

int usable_gb(const LogConfig& config);
int flights_capacity(const LogConfig& config);
int rate_tenths(const LogConfig& config);
std::string retention_action(int free_percent, int verified_deletable, const LogConfig& config);
std::vector<std::string> record_command(const LogConfig& config);

}  // namespace drone_logging
