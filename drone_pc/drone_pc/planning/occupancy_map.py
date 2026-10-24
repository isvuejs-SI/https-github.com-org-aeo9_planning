from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import List, Optional, Tuple

from drone_pc.params import Params


@dataclass
class Point3:
    x: float = 0.0
    y: float = 0.0
    z: float = 0.0


@dataclass
class MovingObstacle:
    position: Point3
    label: str = "other"
    radius_m: float = 1.0


@dataclass
class Polygon2:
    vertices: List[Point3] = field(default_factory=list)


class OccupancyMap:
    def __init__(self, params: Params):
        self.resolution = params.map_resolution_m
        self.size_x = params.map_size_x
        self.size_y = params.map_size_y
        self.size_z = params.map_size_z
        self.origin_x = params.map_origin_x_m
        self.origin_y = params.map_origin_y_m
        self.origin_z = params.map_origin_z_m
        self.occupied_threshold = params.map_occupied_threshold
        self.inflate_m = params.map_moving_obstacle_inflate_m
        self.occupancy = [0.0] * (self.size_x * self.size_y * self.size_z)
        self.nofly: List[Polygon2] = []
        self.geofence: Optional[Polygon2] = None

    def _index(self, ix: int, iy: int, iz: int) -> int:
        return (iz * self.size_y + iy) * self.size_x + ix

    def clear(self) -> None:
        self.occupancy = [0.0] * len(self.occupancy)
        self.nofly = []
        self.geofence = None

    def world_to_index(self, p: Point3) -> Optional[Tuple[int, int, int]]:
        ix = int(math.floor((p.x - self.origin_x) / self.resolution))
        iy = int(math.floor((p.y - self.origin_y) / self.resolution))
        iz = int(math.floor((p.z - self.origin_z) / self.resolution))
        if 0 <= ix < self.size_x and 0 <= iy < self.size_y and 0 <= iz < self.size_z:
            return ix, iy, iz
        return None

    def index_to_world(self, ix: int, iy: int, iz: int) -> Point3:
        return Point3(
            self.origin_x + (ix + 0.5) * self.resolution,
            self.origin_y + (iy + 0.5) * self.resolution,
            self.origin_z + (iz + 0.5) * self.resolution,
        )

    def integrate_points(self, points: List[Point3]) -> None:
        for p in points:
            idx = self.world_to_index(p)
            if idx is None:
                continue
            i = self._index(*idx)
            self.occupancy[i] = min(1.0, self.occupancy[i] + 0.35)

    def add_moving_obstacles(self, obstacles: List[MovingObstacle]) -> None:
        inflate = int(math.ceil(self.inflate_m / self.resolution))
        for obs in obstacles:
            center = self.world_to_index(obs.position)
            if center is None:
                continue
            cx, cy, cz = center
            r = max(inflate, int(math.ceil(obs.radius_m / self.resolution)))
            for dz in range(-r, r + 1):
                for dy in range(-r, r + 1):
                    for dx in range(-r, r + 1):
                        if dx * dx + dy * dy + dz * dz > r * r:
                            continue
                        ix, iy, iz = cx + dx, cy + dy, cz + dz
                        if 0 <= ix < self.size_x and 0 <= iy < self.size_y and 0 <= iz < self.size_z:
                            self.occupancy[self._index(ix, iy, iz)] = 1.0

    def set_nofly_polygons(self, polygons: List[Polygon2]) -> None:
        self.nofly = list(polygons)

    def set_geofence(self, fence: Polygon2) -> None:
        self.geofence = fence if fence.vertices else None

    @staticmethod
    def point_in_polygon(x: float, y: float, poly: Polygon2) -> bool:
        v = poly.vertices
        n = len(v)
        if n < 3:
            return False
        inside = False
        j = n - 1
        for i in range(n):
            xi, yi = v[i].x, v[i].y
            xj, yj = v[j].x, v[j].y
            intersect = ((yi > y) != (yj > y)) and (
                x < (xj - xi) * (y - yi) / ((yj - yi) + 1e-12) + xi
            )
            if intersect:
                inside = not inside
            j = i
        return inside

    def occupied(self, ix: int, iy: int, iz: int) -> bool:
        if not (0 <= ix < self.size_x and 0 <= iy < self.size_y and 0 <= iz < self.size_z):
            return True
        return self.occupancy[self._index(ix, iy, iz)] >= self.occupied_threshold

    def occupied_world(self, p: Point3) -> bool:
        idx = self.world_to_index(p)
        if idx is None:
            return True
        return self.occupied(*idx)

    def inside_geofence(self, p: Point3) -> bool:
        if self.geofence is None:
            return True
        return self.point_in_polygon(p.x, p.y, self.geofence)

    def in_nofly(self, p: Point3) -> bool:
        return any(self.point_in_polygon(p.x, p.y, poly) for poly in self.nofly)

    def is_traversable(self, ix: int, iy: int, iz: int) -> bool:
        if self.occupied(ix, iy, iz):
            return False
        w = self.index_to_world(ix, iy, iz)
        if not self.inside_geofence(w):
            return False
        if self.in_nofly(w):
            return False
        return True
