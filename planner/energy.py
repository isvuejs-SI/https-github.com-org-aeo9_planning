"""Integer energy step. E_next = min(C, max(0, E - draw + solar))."""


def energy_next(energy, cap, draw, solar):
    val = energy - draw + solar
    if val < 0:
        val = 0
    if val > cap:
        val = cap
    return val


def launch_gate(energy, need, reserve):
    """Launch only when E >= E_need + E_reserve."""
    return 1 if energy >= need + reserve else 0


def solar_at(t, peak, sunrise, sunset, noon, cloud):
    """Triangle daylight curve. Illustrative, not a measured panel model."""
    if t <= sunrise or t >= sunset:
        return 0
    half = (sunset - sunrise) // 2
    return peak * (half - abs(t - noon)) * cloud // (half * 100)
