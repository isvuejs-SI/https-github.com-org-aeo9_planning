from __future__ import annotations

import math
from dataclasses import dataclass

from drone_pc.params import Params
from drone_pc.planning.occupancy_map import OccupancyMap, Point3


def _dist(a: Point3, b: Point3) -> float:
    dx, dy, dz = a.x - b.x, a.y - b.y, a.z - b.z
    return math.sqrt(dx * dx + dy * dy + dz * dz)


def _clamp_alt(p: Point3, min_alt: float, max_alt: float) -> Point3:
    return Point3(p.x, p.y, min(max(p.z, min_alt), max_alt))


@dataclass
class LocalPlanInput:
    position: Point3
    goal: Point3
    home: Point3
    current_speed_mps: float = 0.0


@dataclass
class LocalPlanOutput:
    setpoint: Point3
    speed_mps: float = 0.0
    stop: bool = False
    reason: str = ""


class LocalPlanner:
    def __init__(self, params: Params):
        self.params = params

    def plan(self, mmap: OccupancyMap, inp: LocalPlanInput) -> LocalPlanOutput:
        goal = _clamp_alt(inp.goal, self.params.local_min_altitude_m, self.params.local_max_altitude_m)
        pos = inp.position

        if _dist(pos, inp.home) > self.params.local_vlos_max_range_m:
            sp = Point3(
                inp.home.x,
                inp.home.y,
                min(max(pos.z, self.params.local_min_altitude_m), self.params.local_max_altitude_m),
            )
            return LocalPlanOutput(sp, self.params.local_min_speed_mps, False, "vlos_limit_return_home_direction")

        dx, dy, dz = goal.x - pos.x, goal.y - pos.y, goal.z - pos.z
        length = max(1e-6, math.sqrt(dx * dx + dy * dy + dz * dz))
        nearest_obs = 1e9
        t = 0.0
        while t <= 1.0:
            s = Point3(pos.x + dx * t, pos.y + dy * t, pos.z + dz * t)
            if mmap.occupied_world(s) or mmap.in_nofly(s) or not mmap.inside_geofence(s):
                nearest_obs = min(nearest_obs, _dist(pos, s))
            t += 0.05

        if nearest_obs <= self.params.local_obstacle_stop_distance_m:
            left = _clamp_alt(
                Point3(
                    pos.x - dy / length * self.params.local_avoidance_lateral_step_m,
                    pos.y + dx / length * self.params.local_avoidance_lateral_step_m,
                    pos.z,
                ),
                self.params.local_min_altitude_m,
                self.params.local_max_altitude_m,
            )
            right = _clamp_alt(
                Point3(
                    pos.x + dy / length * self.params.local_avoidance_lateral_step_m,
                    pos.y - dx / length * self.params.local_avoidance_lateral_step_m,
                    pos.z,
                ),
                self.params.local_min_altitude_m,
                self.params.local_max_altitude_m,
            )
            left_ok = (
                not mmap.occupied_world(left)
                and mmap.inside_geofence(left)
                and not mmap.in_nofly(left)
            )
            right_ok = (
                not mmap.occupied_world(right)
                and mmap.inside_geofence(right)
                and not mmap.in_nofly(right)
            )
            if left_ok:
                return LocalPlanOutput(left, self.params.local_min_speed_mps, False, "avoid_left")
            if right_ok:
                return LocalPlanOutput(right, self.params.local_min_speed_mps, False, "avoid_right")
            return LocalPlanOutput(pos, 0.0, True, "obstacle_stop")

        speed = self.params.local_max_speed_mps
        reason = "clear"
        if nearest_obs <= self.params.local_obstacle_slow_distance_m:
            span = (
                self.params.local_obstacle_slow_distance_m
                - self.params.local_obstacle_stop_distance_m
            )
            tnorm = min(
                1.0,
                max(
                    0.0,
                    (nearest_obs - self.params.local_obstacle_stop_distance_m) / span,
                ),
            )
            speed = self.params.local_min_speed_mps + tnorm * (
                self.params.local_max_speed_mps - self.params.local_min_speed_mps
            )
            reason = "obstacle_slow"

        step = min(length, speed)
        setpoint = _clamp_alt(
            Point3(pos.x + dx / length * step, pos.y + dy / length * step, pos.z + dz / length * step),
            self.params.local_min_altitude_m,
            self.params.local_max_altitude_m,
        )
        return LocalPlanOutput(setpoint, speed, False, reason)
