"""Start the supervisor nodes. They publish requests and never actuators."""

import os

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, SetEnvironmentVariable
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    limits = os.path.join(os.path.dirname(__file__), "..", "config", "limits.yaml")
    return LaunchDescription(
        [
            DeclareLaunchArgument("limits_file", default_value=limits),
            SetEnvironmentVariable(name="DRONE_PC_LIMITS", value=LaunchConfiguration("limits_file")),
            Node(package="drone_pc", executable="planner_node", name="planner_node", output="screen"),
            Node(package="drone_pc", executable="safety_node", name="safety_node", output="screen"),
            Node(package="drone_pc", executable="offboard_node", name="offboard_node", output="screen"),
        ]
    )
