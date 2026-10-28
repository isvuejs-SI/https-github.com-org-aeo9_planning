"""Start the safety supervisor for a PX4 SITL session.

PX4 itself is not launched by this file. Start PX4 SITL separately and point
the companion at it with MAVSDK_CONNECTION or the uXRCE-DDS agent. This file
does not arm a vehicle and does not authorize a real flight.
"""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    share = get_package_share_directory("drone_safety_ops")
    params = os.path.join(share, "config", "params.yaml")
    return LaunchDescription([
        Node(
            package="drone_safety_ops",
            executable="safety_supervisor_node",
            name="safety_supervisor_node",
            parameters=[{"params_file": params, "send_alerts": True}],
            output="screen",
        )
    ])
