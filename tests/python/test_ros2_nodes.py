"""AEO-12: ROS 2 bridge stubs."""

from sim.ros2_nodes import MockRos2Bridge, SupervisorStatus


def test_ros2_battery_image_status_camera():
    b = MockRos2Bridge()
    assert "ROS2_BATTERY" in b.publish_battery(80, 20000)
    assert "ok=1" in b.publish_image(640, 480, 1, True)
    assert b.trigger_camera().startswith("CAMERA_TRIGGER")
    status = SupervisorStatus("NOMINAL", 1000, 3000, True)
    assert "ROS2_STATUS" in b.publish_status(status)
    b.publish_image(640, 480, 2, False)
    assert b.trigger_camera() == "CAMERA_TRIGGER_FAIL"
