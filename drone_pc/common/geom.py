"""Polygon tests on integer millimeter coordinates."""

from __future__ import annotations


def point_in_polygon(x: int, y: int, polygon: list[tuple[int, int]]) -> bool:
    """Even-odd ray cast. Points on an edge are outside."""
    count = len(polygon)
    if count < 3:
        return False
    inside = False
    for index in range(count):
        x1, y1 = polygon[index]
        x2, y2 = polygon[(index + 1) % count]
        if (y1 > y) != (y2 > y):
            lhs = (x - x1) * (y2 - y1)
            rhs = (x2 - x1) * (y - y1)
            hit = lhs < rhs if (y2 - y1) > 0 else lhs > rhs
            if hit:
                inside = not inside
    return inside
