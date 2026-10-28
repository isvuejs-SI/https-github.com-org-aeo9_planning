"""Subscribe to a PointCloud2 and thermal JSON. No vendor sensor drivers."""

from __future__ import annotations

import sys

from drone_pc.common.params import default_limits_path, load_limits
from drone_pc.planning.occupancy import (
    OccupancyGrid,
    apply_thermal,
    decode_pointcloud2_xyz,
    parse_thermal_json,
)


def make_grid(limits):
    return OccupancyGrid(
        (limits.grid_origin_x_m, limits.grid_origin_y_m, limits.grid_origin_z_m),
        limits.grid_resolution_m,
        (limits.grid_size_x, limits.grid_size_y, limits.grid_size_z),
        limits.grid_inflation_m,
    )


def on_pointcloud(grid, message) -> None:
    points = decode_pointcloud2_xyz(
        bytes(message.data),
        message.point_step,
        message.row_step,
        message.width,
        message.height,
        _field_offset(message, "x"),
        _field_offset(message, "y"),
        _field_offset(message, "z"),
    )
    grid.replace_static(points)


def _field_offset(message, name: str) -> int:
    for field in message.fields:
        if field.name == name:
            return int(field.offset)
    raise KeyError(name)


def on_thermal(grid, limits, text: str) -> None:
    detections = parse_thermal_json(text)
    apply_thermal(
        grid,
        detections,
        limits.grid_inflation_m,
        limits.grid_predict_steps,
        limits.grid_predict_dt_s,
    )


def main(argv=None) -> int:
    del argv
    try:
        import rclpy
        from rclpy.node import Node
        from sensor_msgs.msg import PointCloud2
        from std_msgs.msg import String
    except ImportError:
        sys.stderr.write("rclpy is not installed; planner logic is available without the node.\n")
        return 0

    limits = load_limits(default_limits_path())
    rclpy.init()
    node = Node("planner_node")
    grid = make_grid(limits)

    def cloud_cb(message):
        on_pointcloud(grid, message)

    def thermal_cb(message):
        on_thermal(grid, limits, message.data)

    node.create_subscription(PointCloud2, limits.topic_pointcloud, cloud_cb, 10)
    node.create_subscription(String, limits.topic_thermal, thermal_cb, 10)
    try:
        rclpy.spin(node)
    finally:
        node.destroy_node()
        rclpy.shutdown()
    return 0
