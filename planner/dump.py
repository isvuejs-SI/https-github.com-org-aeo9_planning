"""Deterministic supervisor dump — must match C++ aeo9_dump byte-for-byte."""

from __future__ import annotations

import argparse
from pathlib import Path

from control.animal_escape import AnimalEscapeController
from control.failsafe import FailsafeMachine, FailsafeState
from control.gear import GearController
from control.geofence import Geofence
from control.low_battery import should_return_low_battery
from planner.config_loader import load_config
from planner.energy import apply_energy_step, launch_gate
from planner.go_nogo import evaluate_go_nogo
from planner.mission import plan_day
from sim.logger import CsvRunLogger
from sim.px4_adapter import MockPx4Adapter
from sim.ros2_nodes import MockRos2Bridge, SupervisorStatus


def default_config_path() -> Path:
    return Path(__file__).resolve().parents[1] / "config" / "aeo9.yaml"


def build_dump(config_path: Path) -> list[str]:
    cfg = load_config(config_path)
    lines: list[str] = ["AEO9_SUPERVISOR_V1"]
    lines.extend(cfg.dump_lines())

    # Energy step demo
    e0 = cfg.capacity_mwh // 2
    e1 = apply_energy_step(
        e0, cfg.capacity_mwh, cfg.hover_draw_mwh_per_s, cfg.solar_charge_mwh_per_s
    )
    lines.append(
        f"energy_step E={e0} draw={cfg.hover_draw_mwh_per_s} "
        f"solar={cfg.solar_charge_mwh_per_s} -> E={e1}"
    )

    plan = plan_day(cfg)
    lines.extend(plan.dump_lines())

    ok, msg = launch_gate(e1, plan.total_energy_mwh, cfg.reserve_energy_mwh)
    lines.append(f"launch_gate ok={1 if ok else 0} {msg}")

    decision = evaluate_go_nogo(
        wind_cm_s=800,
        max_wind_cm_s=cfg.max_wind_cm_s,
        energy_mwh=e1,
        energy_need_mwh=plan.total_energy_mwh,
        reserve_mwh=cfg.reserve_energy_mwh,
        schedule_ok=True,
        inside_fence=True,
    )
    lines.append(decision.dump_line())

    fs = FailsafeMachine()
    fs.request(FailsafeState.HOLD)
    fs.request(FailsafeState.NOMINAL)  # latched — must stay HOLD
    fs.request(FailsafeState.RTL)
    lines.append(fs.dump_line())

    fence = Geofence(0, 0, cfg.geofence_radius_cm)
    lines.append(fence.dump_line(1000, 1000))
    lines.append(fence.dump_line(cfg.geofence_radius_cm + 100, 0))

    animal = AnimalEscapeController(
        climb_cm=cfg.animal_escape_climb_cm,
        stop_after=cfg.animal_escape_stop_after,
    )
    for _ in range(3):
        lines.append(animal.on_detection(True, True))
    lines.append(animal.dump_line())

    low, low_msg = should_return_low_battery(
        energy_mwh=cfg.reserve_energy_mwh + cfg.return_leg_mwh,
        reserve_mwh=cfg.reserve_energy_mwh,
        return_cost_mwh=cfg.return_leg_mwh,
    )
    lines.append(f"low_battery rtl={1 if low else 0} {low_msg}")

    gear = GearController()
    lines.append(gear.arm()[1])
    lines.append(gear.deploy()[1])  # refused — not landed path after arm
    gear.land()
    lines.append(gear.deploy()[1])
    lines.append(gear.stow()[1])
    lines.append(gear.arm()[1])
    lines.append(gear.dump_line())

    px4 = MockPx4Adapter()
    lines.append(px4.arm())
    lines.append(px4.takeoff(cfg.min_altitude_agl_cm))
    lines.append(px4.goto(1000, 2000, cfg.min_altitude_agl_cm))
    lines.append(px4.hold())
    lines.append(px4.return_to_launch())
    lines.append(px4.land())

    ros = MockRos2Bridge()
    lines.append(ros.publish_battery(90, 21000))
    lines.append(ros.publish_image(640, 480, 0, True))
    lines.append(ros.trigger_camera())
    lines.append(
        ros.publish_status(
            SupervisorStatus(
                state=fs.state.value,
                energy_mwh=e1,
                altitude_cm=cfg.min_altitude_agl_cm,
                filming=animal.filming_allowed,
            )
        )
    )
    lines.append(ros.set_starlink(False))

    lines.append("END")
    return lines


def main() -> None:
    parser = argparse.ArgumentParser(description="AEO-9 deterministic dump")
    parser.add_argument(
        "--config",
        type=Path,
        default=default_config_path(),
        help="Path to aeo9.yaml",
    )
    parser.add_argument(
        "--log",
        type=Path,
        default=None,
        help="Optional CSV log path under logs/",
    )
    args = parser.parse_args()
    lines = build_dump(args.config)
    print("\n".join(lines))

    if args.log is not None:
        logger = CsvRunLogger(args.log)
        logger.log(0, "NOMINAL", 100000, 0)
        logger.log(1000, "HOLD", 99000, 3000)
        logger.flush()


if __name__ == "__main__":
    main()
