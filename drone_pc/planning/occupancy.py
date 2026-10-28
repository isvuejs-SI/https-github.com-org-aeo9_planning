"""3D occupancy grid filled from point lists and moving detections.

A ROS node can decode sensor_msgs/PointCloud2 and call these functions.
No Livox SDK is linked here.
"""

from __future__ import annotations

import json
import struct

from drone_pc.common.units import div_round, floor_div, scale_milli

_DYNAMIC = 2
_STATIC = 1


class OccupancyGrid:
    def __init__(self, origin_m, resolution_m: float, size, inflation_m: float):
        self.origin = [scale_milli(float(value)) for value in origin_m]
        self.res = scale_milli(float(resolution_m))
        if self.res <= 0:
            raise ValueError("grid resolution must be positive")
        self.size = [int(size[0]), int(size[1]), int(size[2])]
        self.inflation_m = float(inflation_m)
        count = self.size[0] * self.size[1] * self.size[2]
        self.cells = [0] * count

    def index(self, ix: int, iy: int, iz: int) -> int:
        return ix + self.size[0] * (iy + self.size[1] * iz)

    def in_bounds(self, ix: int, iy: int, iz: int) -> bool:
        return (
            0 <= ix < self.size[0]
            and 0 <= iy < self.size[1]
            and 0 <= iz < self.size[2]
        )

    def cell_of_mm(self, x_mm: int, y_mm: int, z_mm: int):
        ix = floor_div(x_mm - self.origin[0], self.res)
        iy = floor_div(y_mm - self.origin[1], self.res)
        iz = floor_div(z_mm - self.origin[2], self.res)
        if not self.in_bounds(ix, iy, iz):
            return None
        return (ix, iy, iz)

    def cell_of_m(self, x_m: float, y_m: float, z_m: float):
        return self.cell_of_mm(scale_milli(x_m), scale_milli(y_m), scale_milli(z_m))

    def cell_center(self, ix: int, iy: int, iz: int):
        half = self.res // 2
        return (
            self.origin[0] + ix * self.res + half,
            self.origin[1] + iy * self.res + half,
            self.origin[2] + iz * self.res + half,
        )

    def mark(self, ix: int, iy: int, iz: int, value: int) -> None:
        if not self.in_bounds(ix, iy, iz):
            return
        slot = self.index(ix, iy, iz)
        current = self.cells[slot]
        if value == _STATIC or current == 0:
            self.cells[slot] = value
        elif value == _DYNAMIC and current != _STATIC:
            self.cells[slot] = _DYNAMIC

    def clear_value(self, value: int) -> None:
        for index, current in enumerate(self.cells):
            if current == value:
                self.cells[index] = 0

    def mark_ball(self, x_mm: int, y_mm: int, z_mm: int, radius_mm: int, value: int) -> None:
        contained = self.cell_of_mm(x_mm, y_mm, z_mm)
        if contained is not None:
            self.mark(contained[0], contained[1], contained[2], value)
        if radius_mm <= 0:
            return
        res = self.res
        ix0 = floor_div(x_mm - radius_mm - self.origin[0], res)
        iy0 = floor_div(y_mm - radius_mm - self.origin[1], res)
        iz0 = floor_div(z_mm - radius_mm - self.origin[2], res)
        ix1 = floor_div(x_mm + radius_mm - self.origin[0], res)
        iy1 = floor_div(y_mm + radius_mm - self.origin[1], res)
        iz1 = floor_div(z_mm + radius_mm - self.origin[2], res)
        ix0 = max(0, ix0)
        iy0 = max(0, iy0)
        iz0 = max(0, iz0)
        ix1 = min(self.size[0] - 1, ix1)
        iy1 = min(self.size[1] - 1, iy1)
        iz1 = min(self.size[2] - 1, iz1)
        radius_sq = radius_mm * radius_mm
        half = res // 2
        for iz in range(iz0, iz1 + 1):
            cz = self.origin[2] + iz * res + half
            dz = cz - z_mm
            for iy in range(iy0, iy1 + 1):
                cy = self.origin[1] + iy * res + half
                dy = cy - y_mm
                for ix in range(ix0, ix1 + 1):
                    cx = self.origin[0] + ix * res + half
                    dx = cx - x_mm
                    if dx * dx + dy * dy + dz * dz <= radius_sq:
                        self.mark(ix, iy, iz, value)

    def insert_points_m(self, points, value: int = _STATIC) -> None:
        radius = scale_milli(self.inflation_m)
        for point in points:
            self.mark_ball(
                scale_milli(float(point[0])),
                scale_milli(float(point[1])),
                scale_milli(float(point[2])),
                radius,
                value,
            )

    def replace_static(self, points) -> None:
        self.clear_value(_STATIC)
        self.insert_points_m(points, _STATIC)

    def occupied(self) -> list[list[int]]:
        found = []
        for iz in range(self.size[2]):
            for iy in range(self.size[1]):
                for ix in range(self.size[0]):
                    value = self.cells[self.index(ix, iy, iz)]
                    if value:
                        found.append([ix, iy, iz, value])
        return found


def mark_moving(grid: OccupancyGrid, detection: dict, inflation_m: float, steps: int, dt_s: float) -> None:
    radius_mm = scale_milli(float(detection["radius_m"])) + scale_milli(float(inflation_m))
    x0 = scale_milli(float(detection["x"]))
    y0 = scale_milli(float(detection["y"]))
    z0 = scale_milli(float(detection["z"]))
    vx = scale_milli(float(detection["vx"]))
    vy = scale_milli(float(detection["vy"]))
    vz = scale_milli(float(detection["vz"]))
    dt_milli = scale_milli(float(dt_s))
    count = int(steps)
    for step in range(count + 1):
        px = x0 + div_round(vx * dt_milli * step, 1000)
        py = y0 + div_round(vy * dt_milli * step, 1000)
        pz = z0 + div_round(vz * dt_milli * step, 1000)
        grid.mark_ball(px, py, pz, radius_mm, _DYNAMIC)


def apply_thermal(grid: OccupancyGrid, detections, inflation_m: float, steps: int, dt_s: float) -> None:
    grid.clear_value(_DYNAMIC)
    for detection in detections:
        kind = str(detection["class"])
        if kind not in ("person", "animal"):
            continue
        mark_moving(grid, detection, inflation_m, steps, dt_s)


def parse_thermal_json(text: str) -> list[dict]:
    payload = json.loads(text)
    parsed = []
    for detection in payload["detections"]:
        parsed.append(
            {
                "class": str(detection["class"]),
                "x": float(detection["x"]),
                "y": float(detection["y"]),
                "z": float(detection["z"]),
                "vx": float(detection["vx"]),
                "vy": float(detection["vy"]),
                "vz": float(detection["vz"]),
                "radius_m": float(detection["radius_m"]),
            }
        )
    return parsed


def decode_pointcloud2_xyz(
    data: bytes,
    point_step: int,
    row_step: int,
    width: int,
    height: int,
    x_offset: int,
    y_offset: int,
    z_offset: int,
) -> list[tuple[float, float, float]]:
    points = []
    for row in range(int(height)):
        for col in range(int(width)):
            base = row * int(row_step) + col * int(point_step)
            x_m = struct.unpack_from("<f", data, base + int(x_offset))[0]
            y_m = struct.unpack_from("<f", data, base + int(y_offset))[0]
            z_m = struct.unpack_from("<f", data, base + int(z_offset))[0]
            points.append((float(x_m), float(y_m), float(z_m)))
    return points
