"""AEO-5: go/no-go ordered checks."""

from planner.go_nogo import GateResult, evaluate_go_nogo


def test_order_wind_battery_schedule_fence():
    wind = evaluate_go_nogo(
        wind_cm_s=2000,
        max_wind_cm_s=1200,
        energy_mwh=10,
        energy_need_mwh=1,
        reserve_mwh=1,
        schedule_ok=False,
        inside_fence=False,
    )
    assert wind.result == GateResult.WAIT
    assert wind.reason.startswith("wind=")

    batt = evaluate_go_nogo(
        wind_cm_s=100,
        max_wind_cm_s=1200,
        energy_mwh=10,
        energy_need_mwh=50,
        reserve_mwh=50,
        schedule_ok=False,
        inside_fence=False,
    )
    assert batt.result == GateResult.WAIT
    assert "battery" in batt.reason

    sched = evaluate_go_nogo(
        wind_cm_s=100,
        max_wind_cm_s=1200,
        energy_mwh=200,
        energy_need_mwh=50,
        reserve_mwh=50,
        schedule_ok=False,
        inside_fence=False,
    )
    assert sched.result == GateResult.SKIP
    assert sched.reason == "schedule_closed"

    fence = evaluate_go_nogo(
        wind_cm_s=100,
        max_wind_cm_s=1200,
        energy_mwh=200,
        energy_need_mwh=50,
        reserve_mwh=50,
        schedule_ok=True,
        inside_fence=False,
    )
    assert fence.result == GateResult.SKIP
    assert fence.reason == "outside_fence"

    go = evaluate_go_nogo(
        wind_cm_s=100,
        max_wind_cm_s=1200,
        energy_mwh=200,
        energy_need_mwh=50,
        reserve_mwh=50,
        schedule_ok=True,
        inside_fence=True,
    )
    assert go.result == GateResult.GO
