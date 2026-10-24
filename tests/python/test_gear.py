"""AEO-10: gear/solar stow-arm-deploy rules."""

from control.gear import GearActionResult, GearController


def test_stow_before_arm_deploy_after_land():
    g = GearController()
    assert g.arm()[0] == GearActionResult.OK
    assert g.deploy()[0] == GearActionResult.REFUSED
    g.land()
    assert g.deploy()[0] == GearActionResult.OK
    assert g.arm()[0] == GearActionResult.REFUSED
    g.stow()
    assert g.arm()[0] == GearActionResult.OK
