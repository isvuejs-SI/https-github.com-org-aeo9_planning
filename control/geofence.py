"""Distance from home, integer meters. Breach is strictly outside the radius."""


def dist2(x, y):
    return x * x + y * y


def breached(x, y, radius):
    return 1 if dist2(x, y) > radius * radius else 0
