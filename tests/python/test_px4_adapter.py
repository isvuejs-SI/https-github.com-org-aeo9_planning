"""AEO-11: PX4 adapter mock."""

import pytest

from sim.px4_adapter import MavsdkPx4Adapter, MockPx4Adapter


def test_mock_px4_commands():
    m = MockPx4Adapter()
    assert m.arm() == "PX4_ARM"
    assert "3000" in m.takeoff(3000)
    assert "GOTO" in m.goto(1, 2, 3)
    assert m.hold() == "PX4_HOLD"
    assert m.return_to_launch() == "PX4_RTL"
    assert m.land() == "PX4_LAND"
    assert m.state.history[0] == "PX4_ARM"


def test_real_mavsdk_disabled_sim_only():
    with pytest.raises(RuntimeError, match="sim_only"):
        MavsdkPx4Adapter()
