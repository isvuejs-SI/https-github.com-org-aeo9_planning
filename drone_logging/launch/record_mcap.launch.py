"""rosbag2 recorder using the mcap storage plugin (`ros2 bag record --storage mcap`).

Host unit tests do not launch this file. If ROS 2 is absent, calling
generate_launch_description raises instead of pretending a bag was recorded.
Topic names and the storage budget come from config/logging.yaml.
"""

from __future__ import annotations

from pathlib import Path

from drone_logging.budget import record_command
from drone_logging.queue import load_log_config

CONFIG = Path(__file__).resolve().parents[1] / "config" / "logging.yaml"


def generate_launch_description():
    try:
        from launch import LaunchDescription
        from launch.actions import ExecuteProcess
    except ImportError as exc:
        raise RuntimeError(
            "ROS 2 launch is not installed. The MCAP recorder was not started. "
            "This is not a successful bag recording."
        ) from exc
    command = record_command(load_log_config(str(CONFIG)))
    return LaunchDescription([ExecuteProcess(cmd=command, output="screen")])
