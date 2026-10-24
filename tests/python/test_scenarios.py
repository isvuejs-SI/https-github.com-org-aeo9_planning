"""AEO-13: scenario suite."""

from pathlib import Path

from control.animal_escape import AnimalEscapeController
from control.failsafe import FailsafeMachine, FailsafeState
from control.geofence import Geofence, GeofenceStatus
from control.low_battery import should_return_low_battery
from planner.config_loader import load_config
from planner.energy import apply_energy_step
from planner.go_nogo import GateResult, evaluate_go_nogo
from planner.mission import plan_day
from sim.px4_adapter import MockPx4Adapter
from sim.ros2_nodes import MockRos2Bridge

CFG = Path(__file__).resolve().parents[2] / "config" / "aeo9.yaml"


def test_scenario_nominal():
    cfg = load_config(CFG)
    plan = plan_day(cfg)
    e = cfg.capacity_mwh
    decision = evaluate_go_nogo(
        wind_cm_s=500,
        max_wind_cm_s=cfg.max_wind_cm_s,
        energy_mwh=e,
        energy_need_mwh=plan.total_energy_mwh,
        reserve_mwh=cfg.reserve_energy_mwh,
        schedule_ok=True,
        inside_fence=True,
    )
    assert decision.result == GateResult.GO
    px4 = MockPx4Adapter()
    px4.arm()
    px4.takeoff(cfg.min_altitude_agl_cm)
    assert px4.state.mode == "TAKEOFF"


def test_scenario_gust_wait():
    cfg = load_config(CFG)
    d = evaluate_go_nogo(
        wind_cm_s=cfg.max_wind_cm_s + 1,
        max_wind_cm_s=cfg.max_wind_cm_s,
        energy_mwh=cfg.capacity_mwh,
        energy_need_mwh=1,
        reserve_mwh=1,
        schedule_ok=True,
        inside_fence=True,
    )
    assert d.result == GateResult.WAIT


def test_scenario_low_battery_rtl():
    cfg = load_config(CFG)
    yes, _ = should_return_low_battery(
        cfg.reserve_energy_mwh + cfg.return_leg_mwh,
        cfg.reserve_energy_mwh,
        cfg.return_leg_mwh,
    )
    assert yes is True
    fs = FailsafeMachine()
    assert fs.request(FailsafeState.RTL) == FailsafeState.RTL


def test_scenario_geofence_drift():
    cfg = load_config(CFG)
    fence = Geofence(0, 0, cfg.geofence_radius_cm)
    assert fence.check(cfg.geofence_radius_cm + 50, 0) == GeofenceStatus.BREACH
    fs = FailsafeMachine()
    fs.request(FailsafeState.RTL)


def test_scenario_repeated_animal_escapes():
    cfg = load_config(CFG)
    animal = AnimalEscapeController(cfg.animal_escape_climb_cm, cfg.animal_escape_stop_after)
    for _ in range(cfg.animal_escape_stop_after):
        animal.on_detection(True, True)
    assert animal.filming_allowed is False


def test_scenario_camera_failure():
    ros = MockRos2Bridge()
    ros.publish_image(640, 480, 0, False)
    assert ros.trigger_camera() == "CAMERA_TRIGGER_FAIL"


def test_scenario_starlink_outage():
    ros = MockRos2Bridge()
    assert ros.set_starlink(False) == "STARLINK=DOWN"
    assert ros.starlink_up is False
    # Outage does not flip failsafe by itself; supervisor may HOLD.
    fs = FailsafeMachine()
    assert fs.request(FailsafeState.HOLD) == FailsafeState.HOLD


def test_scenario_energy_drain_over_time():
    cfg = load_config(CFG)
    e = cfg.capacity_mwh
    for _ in range(100):
        e = apply_energy_step(e, cfg.capacity_mwh, cfg.film_draw_mwh_per_s, cfg.solar_charge_mwh_per_s)
    assert 0 <= e <= cfg.capacity_mwh
