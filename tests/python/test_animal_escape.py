"""AEO-8: animal escape climb + stop after 3."""

from control.animal_escape import AnimalEscapeController


def test_animal_escape_stops_filming_after_three():
    c = AnimalEscapeController(climb_cm=3500, stop_after=3)
    assert "filming=1" in c.on_detection(True, True)
    assert "filming=1" in c.on_detection(True, True)
    assert "filming=0" in c.on_detection(True, True)
    assert c.filming_allowed is False
    assert c.escape_count == 3
    assert c.last_climb_cm == 3500
    assert c.on_detection(False, True) == "ANIMAL_NONE"
