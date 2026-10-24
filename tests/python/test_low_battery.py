"""AEO-9: low-battery RTL threshold."""

from control.low_battery import should_return_low_battery


def test_low_battery_threshold():
    yes, msg = should_return_low_battery(80, 50, 30)
    assert yes is True
    assert "LOW_BATTERY_RTL" in msg
    no, msg2 = should_return_low_battery(81, 50, 30)
    assert no is False
    assert "BATTERY_OK" in msg2
