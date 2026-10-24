"""AEO-4: mission planner."""

from pathlib import Path

from planner.config_loader import load_config
from planner.mission import PHASE_ORDER, plan_day

CFG = Path(__file__).resolve().parents[2] / "config" / "aeo9.yaml"


def test_day_plan_phases_and_legs():
    cfg = load_config(CFG)
    plan = plan_day(cfg)
    assert plan.phases == PHASE_ORDER
    assert len(plan.legs) == 5
    assert [leg.leg for leg in plan.legs] == [
        "launch",
        "transit",
        "filming",
        "return",
        "overnight",
    ]
    assert plan.total_energy_mwh == sum(leg.energy_mwh for leg in plan.legs)
    assert plan.total_energy_mwh > 0
