"""NVMe budget and the rosbag2 record command. Does not start a bag."""

from __future__ import annotations

from drone_logging.queue import LogConfig


def usable_gb(config: LogConfig) -> int:
    return config.nvme_size_gb * (100 - config.reserve_free_percent) // 100 - config.os_and_staging_gb


def flights_capacity(config: LogConfig) -> int:
    return usable_gb(config) // config.est_gb_per_flight


def rate_tenths(config: LogConfig) -> int:
    total = config.lidar_mb_s + config.video_mb_s + config.thermal_mb_s + config.other_mb_s
    return int(round(total * 10))


def retention_action(free_percent: int, verified_deletable: int, config: LogConfig) -> str:
    if free_percent < config.stop_nonessential_free_percent:
        if verified_deletable == 0:
            return "STOP_NONESSENTIAL_AND_ALERT"
        return "DELETE_VERIFIED"
    if free_percent < config.delete_when_free_below_percent:
        if verified_deletable > 0:
            return "DELETE_VERIFIED"
        return "HOLD"
    return "OK"


def record_command(config: LogConfig) -> list[str]:
    size_bytes = config.max_bag_size_mb * 1024 * 1024
    return [
        "ros2",
        "bag",
        "record",
        "--storage",
        "mcap",
        "--max-bag-size",
        str(size_bytes),
        *config.topics,
    ]
