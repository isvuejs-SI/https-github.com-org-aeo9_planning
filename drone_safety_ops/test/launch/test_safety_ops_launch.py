"""launch_testing for the safety supervisor.

Requires ROS 2, rclpy, and a built drone_safety_ops package. This file is not
executed in an environment without those packages. A skip is not a pass.
"""

import time
import unittest


def generate_test_description():
    from launch import LaunchDescription
    from launch.actions import TimerAction
    from launch_ros.actions import Node
    from launch_testing.actions import ReadyToTest
    import os
    share_params = os.environ.get("DRONE_SAFETY_PARAMS", "")
    parameters = [{"send_alerts": False}]
    if share_params:
        parameters = [{"params_file": share_params, "send_alerts": False}]
    node = Node(
        package="drone_safety_ops",
        executable="safety_supervisor_node_py",
        name="safety_supervisor_node",
        parameters=parameters,
        output="screen",
    )
    return LaunchDescription([node, TimerAction(period=1.0, actions=[ReadyToTest()])]), {}


class TestSafetySupervisor(unittest.TestCase):
    def test_unconfigured_is_no_go(self, proc_output):
        proc_output.assertWaitFor("worst NO_GO", timeout=20)

    def test_person_inside_5m_is_prop_lockout(self, proc_output):
        import rclpy
        from std_msgs.msg import String

        if not rclpy.ok():
            rclpy.init()
        node = rclpy.create_node("safety_ops_launch_test")
        try:
            pubs = {
                "/drone_safety_ops/battery":
                    '{"soc_pct":80,"cell_temp_c":30,"airborne":false,"armed":false,"props_ok":true,"wind_ms":3,"wind_ok":true}',
                "/drone_safety_ops/gps_geofence":
                    '{"gps_ok":true,"inside_fence":true,"legal_area_ok":true,"geofence_ok":true,"alt_m":0,"dist_home_km":0}',
                "/drone_safety_ops/link_heartbeat":
                    '{"remote_id_ok":true,"observer_ok":true,"phase":"LAUNCH","day":1,"link_gap_s":0}',
                "/drone_safety_ops/lidar": '{"kind":"human","dist_m":3.0,"closing_mps":0}',
                "/drone_safety_ops/thermal": '{"kind":"none","dist_m":0,"closing_mps":0}',
            }
            created = {topic: node.create_publisher(String, topic, 10) for topic in pubs}
            deadline = time.time() + 5
            while time.time() < deadline:
                for topic, payload in pubs.items():
                    msg = String()
                    msg.data = payload
                    created[topic].publish(msg)
                rclpy.spin_once(node, timeout_sec=0.1)
            proc_output.assertWaitFor("PROP_LOCKOUT", timeout=20)
            proc_output.assertWaitFor("px4 no_arm", timeout=20)
        finally:
            node.destroy_node()


if __name__ == "__main__":
    try:
        import launch_testing  # noqa: F401
    except ImportError:
        print("launch_testing: not run (package not installed). This is not a pass.")
        raise SystemExit(77)
    unittest.main()
