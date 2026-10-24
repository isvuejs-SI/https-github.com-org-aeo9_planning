"""ROS 2 supervisor node (Python). Request-only to PX4; PX4 owns failsafes."""

from __future__ import annotations

try:
    import rclpy
    from rclpy.node import Node
    from std_msgs.msg import String
except ImportError:  # pragma: no cover - SITL host without ROS
    rclpy = None
    Node = object  # type: ignore
    String = None  # type: ignore

from drone_pc.params import Params
from drone_pc.planning.state_machine import MissionPhase, StateMachine
from drone_pc.planning.energy_planner import EnergyPlanner
from drone_pc.planning.escape_planner import EscapePlanner
from drone_pc.control.px4_interface import Px4Interface, Px4Request
from drone_pc.control.link_loss import LinkLossMonitor, LinkLossAction
from drone_pc.safety.preflight_gate import PreflightGate
from drone_pc.safety.alerts import AlertManager
from drone_pc.safety.event_logger import EventLogger


class DronePcNode(Node):  # type: ignore[misc]
    def __init__(self) -> None:
        super().__init__("drone_pc_py")
        self.params = Params.defaults()
        self.sm = StateMachine(MissionPhase.SLEEP_CHARGING)
        self.gate = PreflightGate(self.params)
        self.energy = EnergyPlanner(self.params)
        self.escape = EscapePlanner(self.params)
        self.px4 = Px4Interface(self.params)
        self.link = LinkLossMonitor(self.params)
        self.alerts = AlertManager(self.params)
        self.logger = EventLogger(self.params)
        self.status_pub = self.create_publisher(String, "drone_pc/status_py", 10)
        period = self.px4.offboard_period_s()
        self.timer = self.create_timer(period, self._on_timer)
        self.get_logger().info(
            "drone_pc_py online — request-only to PX4; PX4 owns failsafes"
        )

    def _on_timer(self) -> None:
        self.px4.send_heartbeat()
        self.px4.tick(self.px4.offboard_period_s())
        link_res = self.link.update(self.px4.offboard_period_s(), True)
        now = self.get_clock().now().nanoseconds * 1e-9
        if link_res.action == LinkLossAction.RTL:
            self.px4.request(Px4Request.RTL)
            self.alerts.maybe_send("rtl", "link_loss_rtl", now)
            self.logger.log_verdict(now, "RTL:link_loss")
        elif link_res.action == LinkLossAction.ABORT_MISSION:
            self.px4.request(Px4Request.LAND)
            self.alerts.maybe_send("abort", "link_loss_abort", now)
            self.logger.log_verdict(now, "ABORT_MISSION:link_loss")
        msg = String()
        msg.data = self.sm.current.value
        self.status_pub.publish(msg)


def main() -> None:
    if rclpy is None:
        raise SystemExit("rclpy not available — run shared tests instead")
    rclpy.init()
    node = DronePcNode()
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == "__main__":
    main()
