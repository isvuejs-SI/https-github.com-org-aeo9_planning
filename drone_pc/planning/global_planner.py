"""A* on the occupancy grid.

Geofence and no-fly polygons are hard constraints. Neighbor order and the
heap key are fixed so C++ and Python return the same cell list.
"""

from __future__ import annotations

import heapq

from drone_pc.common.geom import point_in_polygon
from drone_pc.common.units import scale_milli
from drone_pc.planning.occupancy import OccupancyGrid

NEIGHBORS = (
    (1, 0, 0),
    (-1, 0, 0),
    (0, 1, 0),
    (0, -1, 0),
    (0, 0, 1),
    (0, 0, -1),
)


def polygons_mm(polygons) -> list[list[tuple[int, int]]]:
    converted = []
    for polygon in polygons:
        converted.append([(scale_milli(x), scale_milli(y)) for x, y in polygon])
    return converted


def polygon_mm(polygon) -> list[tuple[int, int]]:
    if not polygon:
        return []
    return [(scale_milli(x), scale_milli(y)) for x, y in polygon]


def cell_blocked(grid: OccupancyGrid, cell, fence_mm, no_fly_mm, max_alt_mm) -> bool:
    ix, iy, iz = cell
    if not grid.in_bounds(ix, iy, iz):
        return True
    if grid.cells[grid.index(ix, iy, iz)] != 0:
        return True
    cx, cy, cz = grid.cell_center(ix, iy, iz)
    if max_alt_mm is not None and cz > max_alt_mm:
        return True
    if fence_mm and not point_in_polygon(cx, cy, fence_mm):
        return True
    for polygon in no_fly_mm:
        if point_in_polygon(cx, cy, polygon):
            return True
    return False


def plan_path(grid: OccupancyGrid, start, goal, geofence, no_fly, max_alt_m) -> list[list[int]]:
    """Return the cell path from start to goal, or an empty list."""
    fence_mm = polygon_mm(geofence)
    no_fly_mm = polygons_mm(no_fly)
    max_alt_mm = None if max_alt_m is None else scale_milli(float(max_alt_m))
    start_t = (int(start[0]), int(start[1]), int(start[2]))
    goal_t = (int(goal[0]), int(goal[1]), int(goal[2]))

    def blocked(cell):
        return cell_blocked(grid, cell, fence_mm, no_fly_mm, max_alt_mm)

    if blocked(start_t) or blocked(goal_t):
        return []

    def heuristic(cell):
        return (
            abs(cell[0] - goal_t[0]) + abs(cell[1] - goal_t[1]) + abs(cell[2] - goal_t[2])
        ) * grid.res

    open_heap = []
    h0 = heuristic(start_t)
    heapq.heappush(open_heap, (h0, h0, start_t[2], start_t[1], start_t[0]))
    gscore = {start_t: 0}
    came = {start_t: None}
    found = False
    while open_heap:
        f_cost, h_cost, iz, iy, ix = heapq.heappop(open_heap)
        current = (ix, iy, iz)
        g_cost = gscore.get(current)
        if g_cost is None or g_cost + h_cost != f_cost:
            continue
        if current == goal_t:
            found = True
            break
        for dx, dy, dz in NEIGHBORS:
            neighbor = (ix + dx, iy + dy, iz + dz)
            if blocked(neighbor):
                continue
            next_g = g_cost + grid.res
            if next_g < gscore.get(neighbor, 10**18):
                gscore[neighbor] = next_g
                came[neighbor] = current
                next_h = heuristic(neighbor)
                heapq.heappush(
                    open_heap,
                    (next_g + next_h, next_h, neighbor[2], neighbor[1], neighbor[0]),
                )
    if not found:
        return []
    path = []
    cursor = goal_t
    while cursor is not None:
        path.append([cursor[0], cursor[1], cursor[2]])
        cursor = came[cursor]
    path.reverse()
    return path
