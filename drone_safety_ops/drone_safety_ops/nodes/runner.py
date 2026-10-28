"""ROS 2 wrappers. Each verdict role is a node. PX4 receives requests only."""

import json
import os
import time

from drone_safety_ops.aggregate import (
    Cache,
    apply_battery,
    apply_gps,
    apply_lidar,
    apply_link,
    apply_thermal,
    evaluate,
)
from drone_safety_ops.alert import Alerter, send_ntfy
from drone_safety_ops.day3 import render as render_day3
from drone_safety_ops.params import load_params
from drone_safety_ops.px4_request import command_json, dispatch_mavsdk
from drone_safety_ops.verdict_log import VerdictLog, choose_log_dir
from drone_safety_ops.watchdog import notify_ready, notify_watchdog

TOPICS = {
    "battery": "/drone_safety_ops/battery",
    "gps": "/drone_safety_ops/gps_geofence",
    "link": "/drone_safety_ops/link_heartbeat",
    "lidar": "/drone_safety_ops/lidar",
    "thermal": "/drone_safety_ops/thermal",
}


def main_role(role):
    try:
        import rclpy
        from rclpy.qos import DurabilityPolicy, QoSProfile, ReliabilityPolicy
        from std_msgs.msg import String
    except ImportError:
        print("ROS 2 (rclpy) is not installed. Shared verdict tests do not need it.", flush=True)
        return 2

    rclpy.init()
    node = rclpy.create_node(role + "_node")
    app = SafetyApp(role, node, String, QoSProfile, DurabilityPolicy, ReliabilityPolicy)
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.shutdown()
    return 0


class SafetyApp:
    def __init__(self, role, node, string_type, qos_profile, durability, reliability):
        self.role = role
        self.node = node
        self.string_type = string_type
        params_file = node.declare_parameter("params_file", "").value
        self.params = load_params(params_file or None)
        self.send_alerts = bool(node.declare_parameter("send_alerts", True).value)
        self.cache = Cache()
        self.started = time.monotonic()
        self.last_link = None
        self.last_thermal = None
        self.explicit_gap = None
        self.last_px4 = None
        log_root = os.environ.get("DRONE_SAFETY_LOG_DIR") or choose_log_dir(
            self.params.logging.nvme_dir, self.params.logging.fallback_dir
        )
        self.log = VerdictLog(log_root)
        self.alerter = Alerter(self.params.alert.duplicate_holdback_s, sender=self._send_ntfy)
        qos = 10
        latch_qos = qos_profile(depth=1, durability=durability.TRANSIENT_LOCAL, reliability=reliability.RELIABLE)
        self.pubs = {
            "supervisor": node.create_publisher(string_type, "/drone_safety_ops/supervisor/verdict", qos),
            "deploy": node.create_publisher(string_type, "/drone_safety_ops/supervisor/deploy", qos),
            "mission_gate": node.create_publisher(string_type, "/drone_safety_ops/mission_gate/verdict", qos),
            "day3": node.create_publisher(string_type, "/drone_safety_ops/day3/verdict", qos),
            "day3_replay": node.create_publisher(string_type, "/drone_safety_ops/day3/replay", latch_qos),
            "guard": node.create_publisher(string_type, "/drone_safety_ops/guard/verdict", qos),
            "landing": node.create_publisher(string_type, "/drone_safety_ops/landing/verdict", qos),
            "route": node.create_publisher(string_type, "/drone_safety_ops/route/verdict", qos),
            "worst": node.create_publisher(string_type, "/drone_safety_ops/worst_verdict", latch_qos),
            "px4": node.create_publisher(string_type, self.params.px4.request_topic, qos),
            "latch": node.create_publisher(string_type, self.params.px4.no_arm_latch_topic, latch_qos),
            "heartbeat": node.create_publisher(string_type, "/drone_safety_ops/heartbeat", qos),
        }
        for name, topic in TOPICS.items():
            node.create_subscription(string_type, topic, lambda msg, source=name: self.on_sensor(source, msg.data), qos)
        node.create_subscription(string_type, "/drone_safety_ops/guard/verdict", self.on_guard_verdict, qos)
        if role in ("day3", "safety_supervisor"):
            self._publish("day3_replay", render_day3(self.params).rstrip("\n"))
        period = max(0.2, float(self.params.watchdog.period_s))
        node.create_timer(period, self.on_timer)
        if role != "alert":
            self.recompute()
        notify_ready()
        notify_watchdog()
        node.get_logger().info(
            "drone_safety_ops " + role + " ready; PX4 owns failsafes; requests are hold, rtl, land, no_arm"
        )

    def _send_ntfy(self, verdict, msg, _priority):
        topic = os.environ.get("NTFY_TOPIC", "")
        if not topic:
            self.node.get_logger().warning("NTFY_TOPIC unset; alert logged only")
            return False
        return send_ntfy(self.params.alert.ntfy_host, topic, verdict, msg)

    def _publish(self, key, text):
        message = self.string_type()
        message.data = text
        self.pubs[key].publish(message)

    def on_sensor(self, source, data):
        try:
            payload = json.loads(data)
        except json.JSONDecodeError as exc:
            self.node.get_logger().error(f"bad {source} json: {exc}")
            return
        if not isinstance(payload, dict):
            self.node.get_logger().error(f"bad {source} json: expected object")
            return
        now = time.monotonic()
        if source == "battery":
            apply_battery(self.cache, payload)
        elif source == "gps":
            apply_gps(self.cache, payload)
        elif source == "link":
            self.last_link = now
            self.explicit_gap = float(payload["link_gap_s"]) if "link_gap_s" in payload else None
            apply_link(self.cache, payload)
        elif source == "lidar":
            apply_lidar(self.cache, payload)
        elif source == "thermal":
            self.last_thermal = now
            apply_thermal(self.cache, payload)
        if self.role != "alert":
            self.recompute()

    def on_guard_verdict(self, msg):
        if self.role != "alert":
            return
        verdict = msg.data.strip()
        text = f"{verdict}: lidar {self.cache.lidar_kind} {self.cache.lidar_dist} m"
        sent, line = self.alerter.alert(verdict, text, dry=False)
        if line:
            self.node.get_logger().info(line if sent else "alert suppressed: " + verdict)

    def refresh_ages(self):
        now = time.monotonic()
        origin = self.started if self.last_link is None else self.last_link
        measured = now - origin
        if self.explicit_gap is None:
            self.cache.link_gap_s = measured
        else:
            self.cache.link_gap_s = max(measured, self.explicit_gap)
        timeout = self.params.supervisor.detector_timeout_s
        self.cache.detector_ok = self.last_thermal is not None and (now - self.last_thermal) <= timeout

    def on_timer(self):
        notify_watchdog()
        self._publish("heartbeat", "ok")
        if self.role != "alert":
            self.recompute()

    def recompute(self):
        self.refresh_ages()
        snap = evaluate(self.cache, self.params)
        if self.role in ("supervisor", "safety_supervisor"):
            self._publish("supervisor", snap["supervisor"])
            self._publish("deploy", snap["deploy"])
        if self.role in ("mission_gate", "safety_supervisor"):
            self._publish("mission_gate", snap["gate"] + ":" + snap["gate_why"])
        if self.role in ("guard", "safety_supervisor"):
            self._publish("guard", snap["guard"])
        if self.role in ("landing", "safety_supervisor"):
            self._publish("landing", snap["landing"])
        if self.role in ("route", "safety_supervisor"):
            self._publish("route", snap["route"])
        if self.role in ("day3", "safety_supervisor"):
            self._publish("day3", snap["gate"] + ":" + snap["gate_why"])
        if self.role != "safety_supervisor":
            shown = {
                "supervisor": snap["supervisor"],
                "mission_gate": snap["gate"],
                "day3": snap["gate"],
                "guard": snap["guard"],
                "landing": snap["landing"],
                "route": snap["route"],
            }.get(self.role, snap["supervisor"])
            self.node.get_logger().info(f"verdict {self.role} {shown}")
            return
        self._publish("worst", snap["worst"])
        self._publish("latch", "true" if snap["px4"].latch_no_arm else "false")
        signature = (snap["px4"].wire, snap["px4"].latch_no_arm)
        if signature != self.last_px4 and (snap["px4"].wire != "none" or snap["px4"].latch_no_arm):
            payload = command_json(snap["px4"], self.params.px4.uxrce_command_topic)
            self._publish("px4", payload)
            note = dispatch_mavsdk(snap["px4"].wire, self.params.px4.transport)
            self.node.get_logger().info("px4 request " + snap["px4"].wire + " " + note)
        self.last_px4 = signature
        ts = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
        rows = (
            ("supervisor", snap["supervisor"], snap["px4"].wire, ""),
            ("mission_gate", snap["gate"], snap["px4"].wire, snap["gate_why"]),
            ("guard", snap["guard"], snap["px4"].wire, ""),
            ("landing", snap["landing"], snap["px4"].wire, ""),
            ("route", snap["route"], snap["px4"].wire, ""),
            ("worst", snap["worst"], snap["px4"].wire, snap["worst_module"]),
        )
        for module, verdict, px4, reasons in rows:
            self.log.append(ts, module, verdict, px4, reasons)
        if self.send_alerts:
            sent, line = self.alerter.alert(snap["guard"], f"{snap['guard']}: guard verdict", dry=False)
            if line and sent:
                self.node.get_logger().info(line)
        self.node.get_logger().info(f"worst {snap['worst']} px4 {snap['px4'].wire}")
