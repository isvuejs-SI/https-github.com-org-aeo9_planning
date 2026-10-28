"""Short-horizon setpoint: obstacles, speed, altitude, and visual line of sight."""

from __future__ import annotations

from drone_pc.common.params import Limits
from drone_pc.common.units import div_round, hypot_mm, scale_milli
from drone_pc.planning.global_planner import NEIGHBORS, cell_blocked, polygon_mm, polygons_mm
from drone_pc.planning.occupancy import OccupancyGrid


def line_cells(start, goal) -> list[tuple[int, int, int]]:
    ax, ay, az = start
    bx, by, bz = goal
    steps = max(abs(bx - ax), abs(by - ay), abs(bz - az))
    if steps == 0:
        return [start]
    cells = []
    for index in range(steps + 1):
        cell = (
            ax + div_round((bx - ax) * index, steps),
            ay + div_round((by - ay) * index, steps),
            az + div_round((bz - az) * index, steps),
        )
        if not cells or cells[-1] != cell:
            cells.append(cell)
    return cells


def _nearest_occupied_mm(grid: OccupancyGrid, pos) -> int | None:
    best_sq = None
    best_dist = None
    for iz in range(grid.size[2]):
        for iy in range(grid.size[1]):
            for ix in range(grid.size[0]):
                if grid.cells[grid.index(ix, iy, iz)] == 0:
                    continue
                cx, cy, cz = grid.cell_center(ix, iy, iz)
                dx = cx - pos[0]
                dy = cy - pos[1]
                dz = cz - pos[2]
                dist_sq = dx * dx + dy * dy + dz * dz
                if best_sq is None or dist_sq < best_sq:
                    best_sq = dist_sq
                    best_dist = hypot_mm(dx, dy, dz)
    return best_dist


def plan_local(
    grid: OccupancyGrid,
    position_m,
    path,
    home_m,
    mode: str,
    limits: Limits,
    geofence,
    no_fly,
    lookahead_cells: int | None = None,
    vlos_max_m: float | None = None,
) -> dict:
    fence_mm = polygon_mm(geofence)
    no_fly_mm = polygons_mm(no_fly)
    max_alt_mm = scale_milli(limits.alt_max_m)
    lookahead = limits.lookahead_cells if lookahead_cells is None else int(lookahead_cells)
    vlos_mm = scale_milli(limits.vlos_max_range_m if vlos_max_m is None else vlos_max_m)
    pos = (
        scale_milli(float(position_m[0])),
        scale_milli(float(position_m[1])),
        scale_milli(float(position_m[2])),
    )
    home = (
        scale_milli(float(home_m[0])),
        scale_milli(float(home_m[1])),
        scale_milli(float(home_m[2])),
    )
    mode_mm = scale_milli(limits.speed_for_mode(mode))
    cap_mm = scale_milli(limits.speed_max_mps)
    slow_mm = scale_milli(limits.slow_radius_m)
    min_z = scale_milli(limits.alt_min_m)
    max_z = scale_milli(limits.alt_max_m)

    def blocked(cell):
        return cell_blocked(grid, cell, fence_mm, no_fly_mm, max_alt_mm)

    avoiding = False
    hold = False
    chosen = None
    if not path:
        hold = True
    else:
        centers = [grid.cell_center(int(c[0]), int(c[1]), int(c[2])) for c in path]
        best_i = 0
        best_d = None
        for index, center in enumerate(centers):
            dx = center[0] - pos[0]
            dy = center[1] - pos[1]
            dz = center[2] - pos[2]
            dist_sq = dx * dx + dy * dy + dz * dz
            if best_d is None or dist_sq < best_d:
                best_d = dist_sq
                best_i = index
        target_i = min(best_i + lookahead, len(path) - 1)
        target = (int(path[target_i][0]), int(path[target_i][1]), int(path[target_i][2]))
        current = grid.cell_of_mm(pos[0], pos[1], pos[2])
        blocked_leg = current is None
        if current is not None:
            for cell in line_cells(current, target):
                if blocked(cell):
                    blocked_leg = True
                    break
        if blocked_leg:
            avoiding = True
            if current is None:
                hold = True
            else:
                target_center = grid.cell_center(target[0], target[1], target[2])
                best_nb = None
                best_nb_d = None
                for dx, dy, dz in NEIGHBORS:
                    neighbor = (current[0] + dx, current[1] + dy, current[2] + dz)
                    if blocked(neighbor):
                        continue
                    center = grid.cell_center(neighbor[0], neighbor[1], neighbor[2])
                    ddx = center[0] - target_center[0]
                    ddy = center[1] - target_center[1]
                    ddz = center[2] - target_center[2]
                    dist_sq = ddx * ddx + ddy * ddy + ddz * ddz
                    if best_nb is None or dist_sq < best_nb_d:
                        best_nb = neighbor
                        best_nb_d = dist_sq
                if best_nb is None:
                    hold = True
                else:
                    chosen = best_nb
        else:
            chosen = target

    if hold or chosen is None:
        setpoint = pos
        avoiding = True if path else avoiding
    else:
        setpoint = grid.cell_center(chosen[0], chosen[1], chosen[2])

    sx, sy, sz = setpoint
    altitude_clamped = False
    if sz < min_z:
        sz = min_z
        altitude_clamped = True
    if sz > max_z:
        sz = max_z
        altitude_clamped = True

    dx = sx - home[0]
    dy = sy - home[1]
    dist_sq = dx * dx + dy * dy
    vlos_limited = False
    if vlos_mm >= 0 and dist_sq > vlos_mm * vlos_mm:
        dist = hypot_mm(dx, dy, 0)
        if dist <= 0:
            dist = 1
        sx = home[0] + div_round(dx * vlos_mm, dist)
        sy = home[1] + div_round(dy * vlos_mm, dist)
        vlos_limited = True

    if hold or not path:
        speed = 0
    else:
        nearest = _nearest_occupied_mm(grid, pos)
        if nearest is not None and slow_mm > 0 and nearest < slow_mm:
            speed = div_round(mode_mm * nearest, slow_mm)
        else:
            speed = mode_mm
        if speed > cap_mm:
            speed = cap_mm

    return {
        "x_mm": int(sx),
        "y_mm": int(sy),
        "z_mm": int(sz),
        "speed_mm_s": int(speed),
        "vlos_limited": bool(vlos_limited),
        "altitude_clamped": bool(altitude_clamped),
        "avoiding": bool(avoiding),
    }
