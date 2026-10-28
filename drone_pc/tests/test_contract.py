"""Run the shared contract table against the Python stack."""

from __future__ import annotations

import json
import unittest
from dataclasses import replace
from pathlib import Path

from drone_pc.common.params import load_limits
from drone_pc.control.px4_interface import (
    PX4_OWNED_FAILSAFE_PARAMS,
    OffboardLink,
    make_request,
    missed_heartbeat_response,
    requests_for_phase,
    requests_for_verdict,
)
from drone_pc.control.state_machine import StateMachine
from drone_pc.planning.energy import overnight_decision, plan_backup, return_reserve_micro
from drone_pc.planning.escape import evaluate_escape
from drone_pc.planning.global_planner import plan_path
from drone_pc.planning.local_planner import plan_local
from drone_pc.planning.occupancy import (
    OccupancyGrid,
    apply_thermal,
    decode_pointcloud2_xyz,
    parse_thermal_json,
)
from drone_pc.safety.alerts import AlertSink, read_events, write_events
from drone_pc.safety.link_loss import (
    link_loss_action_from_limits,
    mission_event_for_link,
    requests_for_link_action,
)
from drone_pc.safety.preflight import evaluate_preflight, worst_verdict

ROOT = Path(__file__).resolve().parents[1]
CASES = json.loads((ROOT / "tests" / "contract_cases.json").read_text(encoding="utf-8"))


def _near(actual, expected) -> bool:
    if isinstance(expected, bool):
        return actual is expected or actual == expected
    if isinstance(expected, dict):
        if not isinstance(actual, dict) or set(actual.keys()) != set(expected.keys()):
            return False
        return all(_near(actual[key], value) for key, value in expected.items())
    if isinstance(expected, list):
        return (
            isinstance(actual, list)
            and len(actual) == len(expected)
            and all(_near(left, right) for left, right in zip(actual, expected))
        )
    if isinstance(expected, float):
        return isinstance(actual, (int, float)) and not isinstance(actual, bool) and abs(float(actual) - expected) <= 1e-9
    if isinstance(expected, int) and not isinstance(expected, bool):
        return isinstance(actual, (int, float)) and not isinstance(actual, bool) and actual == expected
    return actual == expected


def _limits(case):
    limits = load_limits(str(ROOT / "config" / "limits.yaml"))
    if "override" in case:
        limits = replace(limits, **case["override"])
    return limits


def _grid(case) -> OccupancyGrid:
    return OccupancyGrid(case["origin_m"], case["resolution_m"], case["size"], case["inflation_m"])


def _backup_m(case):
    if case["backup_m"] is None:
        return None
    return tuple(case["backup_m"])


class ContractTest(unittest.TestCase):
    def test_shared_cases(self):
        for case in CASES["cases"]:
            with self.subTest(case=case["id"]):
                self._run(case)

    def _run(self, case):
        op = case["op"]
        if op == "load_limits":
            self.assertTrue(_near(load_limits(str(ROOT / "config" / "limits.yaml")).scalar_map(), case["expect"]))
            return
        if op == "worst_verdict":
            for sample in case["samples"]:
                self.assertEqual(worst_verdict(sample["names"]), sample["expect"])
            return
        if op == "preflight":
            result = evaluate_preflight(
                _limits(case),
                case["wind_mps"],
                case["cell_temp_c"],
                case["soc"],
                case["x_m"],
                case["y_m"],
                case["alt_m"],
                case["remote_id"],
                case["distance_km"],
                case["mission_wh"],
                case["operator_abort"],
            )
            self.assertEqual(result["verdict"], case["expect_verdict"])
            self.assertEqual(result["reason"], case["expect_reason"])
            if "expect_checks" in case:
                self.assertEqual(result["checks"], case["expect_checks"])
            return
        if op == "return_reserve":
            got = return_reserve_micro(
                case["distance_km"],
                case["wh_per_km"],
                case["distance_factor"],
                case["pack_wh"],
                case["pack_fraction"],
            )
            self.assertEqual(got, case["expect_micro_wh"])
            return
        if op == "return_reserve_from_limits":
            limits = _limits(case)
            got = return_reserve_micro(
                case["distance_km"],
                limits.wh_per_km,
                limits.return_distance_factor,
                limits.pack_capacity_wh,
                limits.pack_reserve_fraction,
            )
            self.assertEqual(got, case["expect_micro_wh"])
            return
        if op == "overnight":
            got = overnight_decision(
                case["soc"],
                case["pack_wh"],
                case["solar_wh"],
                case["distance_km"],
                case["mission_wh"],
                case["wh_per_km"],
                case["distance_factor"],
                case["pack_fraction"],
                case["airborne"],
            )
            self.assertEqual(got, case["expect"])
            return
        if op == "backup":
            got = plan_backup(
                case["origin_m"],
                case["candidates"],
                case["blocked_ids"],
                case["geofence"],
                case["no_fly"],
                case["soc"],
                case["pack_wh"],
                case["wh_per_km"],
                case["distance_factor"],
                case["pack_fraction"],
            )
            self.assertTrue(_near(got, case["expect"]))
            return
        if op == "astar":
            grid = _grid(case)
            grid.insert_points_m(case["points_m"])
            path = plan_path(grid, case["start"], case["goal"], case["geofence"], case["no_fly"], case["max_alt_m"])
            self.assertEqual(path, case["expect_path"])
            return
        if op == "local":
            grid = _grid(case)
            grid.insert_points_m(case["points_m"])
            got = plan_local(
                grid,
                case["position_m"],
                case["path"],
                case["home_m"],
                case["mode"],
                _limits(case),
                case["geofence"],
                case["no_fly"],
                case["lookahead_cells"],
                case["vlos_max_m"],
            )
            self.assertTrue(_near(got, case["expect"]))
            return
        if op == "cell_of":
            grid = _grid(case)
            point = case["point_m"]
            self.assertEqual(list(grid.cell_of_m(point[0], point[1], point[2])), case["expect"])
            return
        if op == "pointcloud":
            data = bytes(case["data"])
            points = decode_pointcloud2_xyz(
                data,
                case["point_step"],
                case["row_step"],
                case["width"],
                case["height"],
                case["x_offset"],
                case["y_offset"],
                case["z_offset"],
            )
            self.assertTrue(_near([list(point) for point in points], case["expect_points"]))
            return
        if op == "thermal":
            grid = _grid(case)
            grid.insert_points_m(case["static_points_m"])
            apply_thermal(grid, parse_thermal_json(case["message"]), case["inflation_m"], case["steps"], case["dt_s"])
            self.assertEqual(grid.occupied(), case["expect_occupied"])
            grid.clear_value(2)
            self.assertEqual(grid.occupied(), case["expect_after_clear_dynamic"])
            return
        if op == "escape":
            result = evaluate_escape(
                case["drone_m"],
                case["detections"],
                case["soc"],
                case["cover_open"],
                case["wind_mps"],
                _limits(case),
                _backup_m(case),
            )
            self.assertEqual(result["verdict"], case["expect_verdict"])
            self.assertEqual(result["action"], case["expect_action"])
            self.assertEqual(result["requests"], case["expect_requests"])
            self.assertEqual(result["mission_event"], case["expect_event"])
            self.assertTrue(_near(result["setpoint"], case["expect_setpoint"]))
            return
        if op == "link_loss":
            limits = _limits(case)
            action = link_loss_action_from_limits(case["lost_s"], limits)
            self.assertEqual(action, case["expect_action"])
            self.assertEqual(requests_for_link_action(action), case["expect_requests"])
            self.assertEqual(mission_event_for_link(action), case["expect_event"])
            return
        if op == "state_machine":
            machine = StateMachine(case["start"])
            for event in case["events"]:
                result = machine.handle(event["event"], event["t_ms"])
                self.assertEqual(result["state"], event["expect_state"])
                self.assertEqual(result["reason"], event["expect_reason"])
                if "expect_line" in event:
                    self.assertEqual(result["line"], event["expect_line"])
                else:
                    self.assertIn("kind=transition", result["line"])
                    self.assertIn("reason=" + event["expect_reason"], result["line"])
            return
        if op == "heartbeat":
            link = OffboardLink(case["rate_hz"], case["timeout_s"])
            ticks = [link.tick(now) for now in case["ticks_ms"]]
            self.assertEqual(ticks, case["expect_ticks"])
            missed = [link.missed(now) for now in case["probe_after_last_ms"]]
            self.assertEqual(missed, case["expect_missed"])
            return
        if op == "heartbeat_from_limits":
            limits = _limits(case)
            link = OffboardLink(limits.heartbeat_rate_hz, limits.heartbeat_timeout_s)
            self.assertEqual(link.period_ms, case["expect_period_ms"])
            self.assertEqual(link.timeout_ms, case["expect_timeout_ms"])
            return
        if op == "px4_phases":
            got = {phase: requests_for_phase(phase) for phase in case["expect"]}
            self.assertEqual(got, case["expect"])
            return
        if op == "px4_verdict":
            self.assertEqual(requests_for_verdict(case["verdict"], case["airborne"]), case["expect"])
            return
        if op == "px4_reject":
            with self.assertRaises(ValueError):
                make_request(case["kind"])
            return
        if op == "px4_reject_setpoint":
            with self.assertRaises(ValueError):
                make_request("OFFBOARD_SETPOINT", case["setpoint"])
            return
        if op == "px4_setpoint":
            self.assertEqual(make_request("OFFBOARD_SETPOINT", case["setpoint"]), case["expect"])
            return
        if op == "px4_miss":
            self.assertEqual(missed_heartbeat_response(), case["expect"])
            self.assertEqual(list(PX4_OWNED_FAILSAFE_PARAMS), case["expect_params"])
            return
        if op == "ntfy":
            sink = AlertSink(case["cooldown_s"])
            for step in case["steps"]:
                result = sink.consider(step["t_ms"], "k", "t", "m", case["base_url"], case["topic"])
                self.assertEqual(result["sent"], step["expect_sent"])
                self.assertEqual(result["line"], step["expect_line"])
                if step["expect_sent"]:
                    self.assertEqual(result["request"]["url"], step["expect_url"])
                    self.assertEqual(sorted(result["request"]["body"].keys()), ["message", "title", "topic"])
                    self.assertNotIn("token", result["request"])
            verdict = case["verdict_line"]
            self.assertEqual(sink.log_verdict(verdict["t_ms"], verdict["name"], verdict["reason"]), verdict["expect_line"])
            return
        if op == "log_roundtrip":
            directory = str(ROOT / case["directory"])
            write_events(directory, case["lines"])
            self.assertEqual(read_events(directory), case["lines"])
            return
        raise AssertionError("unknown op " + op)


if __name__ == "__main__":
    unittest.main()
