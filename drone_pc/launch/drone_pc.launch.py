"""Launch drone_pc supervisor for PX4 SITL.

Jetson/companion side only — sends requests; PX4 owns failsafes.
"""

from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
import os


def generate_launch_description():
    pkg = get_package_share_directory("drone_pc")
    params = os.path.join(pkg, "config", "drone_pc_params.yaml")

    return LaunchDescription(
        [
            Node(
                package="drone_pc",
                executable="drone_pc_node",
                name="drone_pc",
                output="screen",
                parameters=[params],
            ),
            Node(
                package="drone_pc",
                executable="drone_pc_py",
                name="drone_pc_py",
                output="screen",
                parameters=[params],
            ),
        ]
    )
