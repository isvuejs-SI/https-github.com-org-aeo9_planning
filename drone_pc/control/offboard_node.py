"""Publish position setpoints and vehicle-command requests. Never actuators."""

from __future__ import annotations

import json
import sys

from drone_pc.common.params import default_limits_path, load_limits
from drone_pc.control.px4_interface import OffboardLink, make_request, missed_heartbeat_response


def main(argv=None) -> int:
    del argv
    try:
        import rclpy
        from rclpy.node import Node
        from std_msgs.msg import String
    except ImportError:
        sys.stderr.write("rclpy is not installed; offboard requests are available without the node.\n")
        return 0

    limits = load_limits(default_limits_path())
    rclpy.init()
    node = Node("offboard_node")
    publisher = node.create_publisher(String, limits.topic_px4_request, 10)
    link = OffboardLink(limits.heartbeat_rate_hz, limits.heartbeat_timeout_s)
    phase = {"name": "SLEEP_CHARGING"}
    setpoint = {"x_mm": 0, "y_mm": 0, "z_mm": 0}

    def on_timer():
        now_ms = int(node.get_clock().now().nanoseconds / 1000000)
        if link.missed(now_ms) and link.last_ms is not None:
            expected = missed_heartbeat_response()
            node.get_logger().warning(
                "offboard heartbeat miss; jetson_action="
                + expected["jetson_action"]
                + " px4_expected="
                + expected["px4_expected"]
            )
        for kind in link.tick(now_ms):
            if phase["name"] in ("SLEEP_CHARGING", "PREFLIGHT_GATE"):
                continue
            body = make_request(kind, setpoint if kind == "OFFBOARD_SETPOINT" else None)
            message = String()
            message.data = json.dumps(body)
            publisher.publish(message)

    period = max(link.period_ms, 1) / 1000.0
    node.create_timer(period, on_timer)
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()
    return 0
