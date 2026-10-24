"""AEO-7: geofence distance + breach latch."""

from control.geofence import Geofence, GeofenceStatus


def test_geofence_inside_then_breach():
    fence = Geofence(0, 0, 50000)
    assert fence.check(1000, 1000) == GeofenceStatus.INSIDE
    assert fence.distance_cm(1000, 0) == 1000
    assert fence.check(50100, 0) == GeofenceStatus.BREACH
    # Latch: still BREACH even if back inside.
    assert fence.check(0, 0) == GeofenceStatus.BREACH
