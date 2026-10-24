from __future__ import annotations

import heapq
import math
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Tuple

from drone_pc.params import Params
from drone_pc.planning.occupancy_map import OccupancyMap, Point3

NodeKey = Tuple[int, int, int]


@dataclass
class GlobalPlanResult:
    success: bool = False
    path: List[Point3] = field(default_factory=list)
    expansions: int = 0
    reason: str = ""


class GlobalPlanner:
    def __init__(self, params: Params):
        self.params = params

    @staticmethod
    def _heuristic(a: NodeKey, b: NodeKey, weight: float) -> float:
        dx, dy, dz = a[0] - b[0], a[1] - b[1], a[2] - b[2]
        return weight * math.sqrt(dx * dx + dy * dy + dz * dz)

    def plan(self, mmap: OccupancyMap, start: Point3, goal: Point3) -> GlobalPlanResult:
        result = GlobalPlanResult()
        s = mmap.world_to_index(start)
        g = mmap.world_to_index(goal)
        if s is None or g is None:
            result.reason = "start_or_goal_out_of_bounds"
            return result
        if not mmap.is_traversable(*s):
            result.reason = "start_not_traversable"
            return result
        if not mmap.is_traversable(*g):
            result.reason = "goal_not_traversable"
            return result

        dirs = [
            (dx, dy, dz)
            for dx in (-1, 0, 1)
            for dy in (-1, 0, 1)
            for dz in (-1, 0, 1)
            if not (dx == 0 and dy == 0 and dz == 0)
        ]

        # (f, x, y, z) — deterministic tie-break matching C++ OpenItem
        open_heap: List[Tuple[float, int, int, int]] = []
        h0 = self._heuristic(s, g, self.params.global_heuristic_weight)
        heapq.heappush(open_heap, (h0, s[0], s[1], s[2]))
        g_score: Dict[NodeKey, float] = {s: 0.0}
        came_from: Dict[NodeKey, NodeKey] = {}
        expansions = 0

        while open_heap and expansions < self.params.global_max_expansions:
            _, x, y, z = heapq.heappop(open_heap)
            current = (x, y, z)
            expansions += 1
            if current == g:
                path_keys = [current]
                while current != s:
                    current = came_from[current]
                    path_keys.append(current)
                path_keys.reverse()
                result.success = True
                result.expansions = expansions
                result.reason = "ok"
                result.path = [mmap.index_to_world(*k) for k in path_keys]
                return result

            g_cur = g_score[current]
            for d in dirs:
                nxt = (current[0] + d[0], current[1] + d[1], current[2] + d[2])
                if not mmap.is_traversable(*nxt):
                    continue
                step = math.sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2])
                tentative = g_cur + step
                if nxt not in g_score or tentative < g_score[nxt]:
                    came_from[nxt] = current
                    g_score[nxt] = tentative
                    f = tentative + self._heuristic(nxt, g, self.params.global_heuristic_weight)
                    heapq.heappush(open_heap, (f, nxt[0], nxt[1], nxt[2]))

        result.expansions = expansions
        result.reason = (
            "max_expansions"
            if expansions >= self.params.global_max_expansions
            else "no_path"
        )
        return result
