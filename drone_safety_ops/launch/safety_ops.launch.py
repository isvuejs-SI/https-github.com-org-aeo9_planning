"""Launch the C++ verdict nodes. Simulation only. PX4 is not started here."""

import os

from ament_index_python.packages import get_package_share_directory
from launch import LaunchDescription
from launch_ros.actions import Node


def generate_launch_description():
    share = get_package_share_directory("drone_safety_ops")
    params = os.path.join(share, "config", "params.yaml")
    roles = ("supervisor", "mission_gate", "day3", "guard", "landing", "route", "alert", "safety_supervisor")
    nodes = []
    for role in roles:
        parameters = [{"params_file": params}]
        if role == "safety_supervisor":
            parameters = [{"params_file": params, "send_alerts": False}]
        nodes.append(Node(
            package="drone_safety_ops",
            executable=role + "_node",
            name=role + "_node",
            parameters=parameters,
            output="screen",
        ))
    return LaunchDescription(nodes)
