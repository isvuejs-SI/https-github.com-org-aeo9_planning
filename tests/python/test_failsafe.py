"""AEO-6: failsafe latch."""

from control.failsafe import FailsafeMachine, FailsafeState


def test_failsafe_latches_no_flip_back():
    m = FailsafeMachine()
    assert m.state == FailsafeState.NOMINAL
    assert m.request(FailsafeState.HOLD) == FailsafeState.HOLD
    assert m.request(FailsafeState.NOMINAL) == FailsafeState.HOLD
    assert m.request(FailsafeState.RTL) == FailsafeState.RTL
    assert m.request(FailsafeState.HOLD) == FailsafeState.RTL
    assert m.request(FailsafeState.LAND) == FailsafeState.LAND
