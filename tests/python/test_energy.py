"""AEO-3: energy model + launch gate."""

from planner.energy import apply_energy_step, launch_gate


def test_energy_step_clamps():
    assert apply_energy_step(100, 200, 10, 5) == 95
    assert apply_energy_step(5, 200, 10, 0) == 0
    assert apply_energy_step(195, 200, 0, 20) == 200


def test_launch_gate():
    ok, msg = launch_gate(150, 80, 50)
    assert ok is True
    assert "LAUNCH_OK" in msg
    blocked, msg2 = launch_gate(100, 80, 50)
    assert blocked is False
    assert "LAUNCH_BLOCKED" in msg2
