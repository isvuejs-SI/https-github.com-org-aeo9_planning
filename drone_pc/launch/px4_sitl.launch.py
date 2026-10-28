"""PX4 SITL path. This file does not arm motors and does not change failsafes.

PX4 must keep its own failsafe parameters. drone_pc does not write them:

- COM_OF_LOSS_T > 0 so a missed offboard setpoint stream makes PX4 Hold or RTL
- COM_OBL_RC_ACT (or the firmware's offboard-loss action) stays Hold (1) or
  RTL, whichever the vehicle config already uses
- NAV_DLL_ACT stays Hold or Return for a datalink loss
- COM_LOW_BAT_ACT stays enabled

Do not set CBRK_* circuit breakers. Do not set a parameter that disables a
failsafe. The Jetson only publishes /drone_pc/px4_request.

Manual SITL (PX4 is not installed in the authoring environment):

    cd PX4-Autopilot
    make px4_sitl gazebo-classic
    # in another shell, with this workspace built:
    ros2 launch drone_pc px4_sitl.launch.py

If PX4_SITL_BIN is set to an existing px4 binary, this launch starts it
with PX4_SYS_AUTOSTART left untouched. It does not pass a parameter file
that weakens failsafes.
"""

import os

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument, ExecuteProcess, LogInfo, SetEnvironmentVariable
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    limits = os.path.join(os.path.dirname(__file__), "..", "config", "limits.yaml")
    sitl_bin = os.environ.get("PX4_SITL_BIN", "")
    actions = [
        DeclareLaunchArgument("limits_file", default_value=limits),
        SetEnvironmentVariable(name="DRONE_PC_LIMITS", value=LaunchConfiguration("limits_file")),
        LogInfo(
            msg=(
                "PX4 owns failsafes. Leave COM_OF_LOSS_T > 0, NAV_DLL_ACT at "
                "Hold or RTL, and COM_LOW_BAT_ACT enabled. drone_pc will not set them."
            )
        ),
        Node(package="drone_pc", executable="planner_node", name="planner_node", output="screen"),
        Node(package="drone_pc", executable="safety_node", name="safety_node", output="screen"),
        Node(package="drone_pc", executable="offboard_node", name="offboard_node", output="screen"),
    ]
    if sitl_bin and os.path.isfile(sitl_bin):
        actions.append(ExecuteProcess(cmd=[sitl_bin], output="screen"))
    else:
        actions.append(
            LogInfo(
                msg=(
                    "PX4 SITL binary not found. Install PX4-Autopilot and run "
                    "`make px4_sitl gazebo-classic`, or set PX4_SITL_BIN. "
                    "No flight is started."
                )
            )
        )
    return LaunchDescription(actions)
