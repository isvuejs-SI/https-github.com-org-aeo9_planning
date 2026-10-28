"""Link-loss requests, preflight logging, and optional ntfy delivery."""

from __future__ import annotations

import json
import sys
import urllib.request

from drone_pc.common.params import default_limits_path, load_limits
from drone_pc.control.px4_interface import make_request
from drone_pc.safety.alerts import AlertSink, build_ntfy, write_events
from drone_pc.safety.link_loss import link_loss_action_from_limits, requests_for_link_action
from drone_pc.safety.preflight import evaluate_preflight


def maybe_send(limits, request: dict) -> None:
    if not limits.ntfy_enable_send or request is None:
        return
    payload = json.dumps(request["body"]).encode("utf-8")
    req = urllib.request.Request(
        request["url"],
        data=payload,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    urllib.request.urlopen(req, timeout=5).close()


def main(argv=None) -> int:
    del argv
    try:
        import rclpy
        from rclpy.node import Node
        from std_msgs.msg import String
    except ImportError:
        sys.stderr.write("rclpy is not installed; safety logic is available without the node.\n")
        return 0

    limits = load_limits(default_limits_path())
    rclpy.init()
    node = Node("safety_node")
    publisher = node.create_publisher(String, limits.topic_px4_request, 10)
    sink = AlertSink(limits.ntfy_cooldown_s)
    down_since = {"ms": None}

    def link_cb(message):
        now_ms = int(node.get_clock().now().nanoseconds / 1000000)
        if message.data.strip() == "up":
            down_since["ms"] = None
            return
        if down_since["ms"] is None:
            down_since["ms"] = now_ms
        lost_s = (now_ms - down_since["ms"]) / 1000.0
        action = link_loss_action_from_limits(lost_s, limits)
        sink.log_verdict(now_ms, action, "link_loss")
        for kind in requests_for_link_action(action):
            body = make_request(kind)
            outgoing = String()
            outgoing.data = json.dumps(body)
            publisher.publish(outgoing)
        if action != "CONTINUE":
            result = sink.consider(
                now_ms,
                "link",
                "drone_pc link",
                action,
                limits.ntfy_base_url,
                limits.ntfy_topic,
            )
            maybe_send(limits, result["request"])
            write_events(limits.log_directory, sink.lines)

    def preflight_cb(message):
        sample = json.loads(message.data)
        result = evaluate_preflight(
            limits,
            float(sample["wind_mps"]),
            float(sample["cell_temp_c"]),
            float(sample["soc"]),
            float(sample["x_m"]),
            float(sample["y_m"]),
            float(sample["alt_m"]),
            str(sample["remote_id"]),
            float(sample["distance_km"]),
            float(sample["mission_wh"]),
            bool(sample["operator_abort"]),
        )
        now_ms = int(node.get_clock().now().nanoseconds / 1000000)
        sink.log_verdict(now_ms, result["verdict"], result["reason"])
        write_events(limits.log_directory, sink.lines)

    node.create_subscription(String, limits.topic_link, link_cb, 10)
    node.create_subscription(String, "/drone_pc/preflight_sample", preflight_cb, 10)
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()
    return 0
