#!/usr/bin/env python3
"""Run shared test_cases.json against Python drone_pc modules."""

from __future__ import annotations

import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT))

from drone_pc.params import Params
from drone_pc.planning.state_machine import MissionPhase, StateMachine
from drone_pc.planning.occupancy_map import OccupancyMap, Point3, MovingObstacle
from drone_pc.planning.global_planner import GlobalPlanner
from drone_pc.planning.local_planner import LocalPlanner, LocalPlanInput
from drone_pc.planning.energy_planner import EnergyPlanner, EnergyPlanInput
from drone_pc.planning.escape_planner import EscapePlanner, EscapeInput, EscapeVerdict
from drone_pc.control.px4_interface import Px4Interface, Px4Request
from drone_pc.control.link_loss import LinkLossMonitor
from drone_pc.safety.preflight_gate import PreflightGate, PreflightInputs, PreflightVerdict
from drone_pc.safety.alerts import AlertManager
from drone_pc.safety.event_logger import EventLogger


def r3(x: float) -> float:
    return round(float(x), 3)


def pt(arr):
    return Point3(float(arr[0]), float(arr[1]), float(arr[2]))


def run_case(case, params: Params):
    mod = case["module"]
    inp = case["input"]
    exp = case["expect"]

    if mod == "state_machine":
        sm = StateMachine(MissionPhase(inp["initial"]))
        oks = []
        for step in inp["steps"]:
            oks.append(sm.transition(MissionPhase(step["to"]), step["reason"]))
        reasons = [h.reason for h in sm.history]
        out = {"final": sm.current.value, "ok": oks, "reasons": reasons}
        return out == exp, out

    if mod == "preflight":
        gate = PreflightGate(params)
        res = gate.evaluate(
            PreflightInputs(
                wind_mps=inp["wind_mps"],
                cell_temp_c=inp["cell_temp_c"],
                soc_percent=inp["soc_percent"],
                geofence_ok=inp["geofence_ok"],
                remote_id_ok=inp["remote_id_ok"],
                return_reserve_ok=inp["return_reserve_ok"],
                airborne=inp["airborne"],
            )
        )
        out = {"verdict": PreflightVerdict(res.verdict).name, "reasons": res.reasons}
        return out == exp, out

    if mod == "energy":
        planner = EnergyPlanner(params)
        res = planner.evaluate(
            EnergyPlanInput(
                distance_to_home_km=inp["distance_to_home_km"],
                soc_percent=inp["soc_percent"],
                remaining_mission_km=inp["remaining_mission_km"],
                overnight=inp["overnight"],
                current_position=pt(inp["current"]),
                candidate_backup_sites=[pt(b) for b in inp["backups"]],
            )
        )
        out = {
            "return_reserve_wh": r3(res.return_reserve_wh),
            "available_wh": r3(res.available_wh),
            "return_reserve_ok": res.return_reserve_ok,
            "stay_overnight": res.stay_overnight,
            "backup_selected": res.backup_selected,
            "backup": [r3(res.backup_site.x), r3(res.backup_site.y), r3(res.backup_site.z)],
            "reason": res.reason,
        }
        exp_n = dict(exp)
        exp_n["return_reserve_wh"] = r3(exp["return_reserve_wh"])
        exp_n["available_wh"] = r3(exp["available_wh"])
        exp_n["backup"] = [r3(x) for x in exp["backup"]]
        return out == exp_n, out

    if mod == "escape_classify":
        planner = EscapePlanner(params)
        verdicts = [planner.classify_threat(d).value for d in inp["distances_m"]]
        out = {"verdicts": verdicts}
        return out == exp, out

    if mod == "escape":
        planner = EscapePlanner(params)
        res = planner.plan(
            EscapeInput(
                threat_verdict=EscapeVerdict(inp["threat"]),
                soc_percent=inp["soc_percent"],
                cover_open=inp["cover_open"],
                wind_ok=inp["wind_ok"],
                current_position=pt(inp["current"]),
                backup_site=pt(inp["backup"]),
                backup_valid=inp["backup_valid"],
            )
        )
        out = {
            "action": res.action.value,
            "execute_escape": res.execute_escape,
            "target": [r3(res.target.x), r3(res.target.y), r3(res.target.z)],
            "reason": res.reason,
        }
        exp_n = dict(exp)
        exp_n["target"] = [r3(x) for x in exp["target"]]
        return out == exp_n, out

    if mod == "link_loss":
        mon = LinkLossMonitor(params)
        actions, reasons = [], []
        for step in inp["steps"]:
            res = mon.update(step["dt_s"], step["link_ok"])
            actions.append(res.action.value)
            reasons.append(res.reason)
        out = {"actions": actions, "reasons": reasons}
        return out == exp, out

    if mod == "alerts":
        am = AlertManager(params)
        sent, reasons = [], []
        for ev in inp["events"]:
            rec = am.maybe_send(ev["key"], ev["message"], ev["t"])
            sent.append(rec.sent)
            reasons.append(rec.reason)
        out = {"sent": sent, "reasons": reasons}
        return out == exp, out

    if mod == "px4":
        iface = Px4Interface(params)
        for name in inp["requests"]:
            iface.request(Px4Request(name))
        iface.tick(inp["heartbeat_gap_s"])
        out = {
            "log": list(iface.request_log),
            "heartbeat_ok": iface.status.heartbeat_ok,
            "fallback": iface.status.fallback,
        }
        return out == exp, out

    if mod == "occupancy":
        mmap = OccupancyMap(params)
        mmap.integrate_points([pt(p) for p in inp["points"]])
        # Ensure lidar hit crosses threshold like a dense return
        mmap.integrate_points([pt(p) for p in inp["points"]])
        mmap.add_moving_obstacles(
            [
                MovingObstacle(pt(m["pos"]), m["label"], m["radius_m"])
                for m in inp["moving"]
            ]
        )
        occupied = [mmap.occupied_world(pt(c)) for c in inp["check"]]
        out = {"occupied": occupied}
        return out == exp, out

    if mod == "global_planner":
        mmap = OccupancyMap(params)
        for b in inp["block"]:
            # Mark cell fully occupied (hard block)
            idx = mmap.world_to_index(pt(b))
            if idx is not None:
                mmap.occupancy[mmap._index(*idx)] = 1.0
        planner = GlobalPlanner(params)
        res = planner.plan(mmap, pt(inp["start"]), pt(inp["goal"]))
        first = [0.0, 0.0, 0.0]
        last = [0.0, 0.0, 0.0]
        if res.path:
            first = [r3(res.path[0].x), r3(res.path[0].y), r3(res.path[0].z)]
            last = [r3(res.path[-1].x), r3(res.path[-1].y), r3(res.path[-1].z)]
        out = {
            "success": res.success,
            "reason": res.reason,
            "path_len": len(res.path),
            "first": first,
            "last": last,
        }
        ok = (
            out["success"] == exp["success"]
            and out["reason"] == exp["reason"]
            and out["path_len"] >= exp["path_len_min"]
            and out["first"] == [r3(x) for x in exp["first"]]
            and out["last"] == [r3(x) for x in exp["last"]]
        )
        return ok, out

    if mod == "local_planner":
        mmap = OccupancyMap(params)
        for b in inp["block"]:
            idx = mmap.world_to_index(pt(b))
            if idx is not None:
                mmap.occupancy[mmap._index(*idx)] = 1.0
        planner = LocalPlanner(params)
        res = planner.plan(
            mmap,
            LocalPlanInput(pt(inp["position"]), pt(inp["goal"]), pt(inp["home"])),
        )
        out = {
            "stop": res.stop,
            "reason": res.reason,
            "speed_mps": r3(res.speed_mps),
        }
        exp_n = {"stop": exp["stop"], "reason": exp["reason"], "speed_mps": r3(exp["speed_mps"])}
        return out == exp_n, out

    if mod == "logger":
        logger = EventLogger(params)
        for v in inp["verdicts"]:
            logger.log_verdict(v["t"], v["payload"])
        for t in inp["transitions"]:
            logger.log_transition(t["t"], t["payload"])
        out = {
            "categories": [e.category for e in logger.events],
            "payloads": [e.payload for e in logger.events],
        }
        return out == exp, out

    raise RuntimeError(f"unknown module {mod}")


def main() -> int:
    cases_path = ROOT / "tests" / "shared" / "test_cases.json"
    data = json.loads(cases_path.read_text(encoding="utf-8"))
    params = Params.defaults()
    failed = 0
    results = []
    for case in data["cases"]:
        ok, out = run_case(case, params)
        results.append({"id": case["id"], "pass": ok, "output": out})
        status = "PASS" if ok else "FAIL"
        print(f"[{status}] {case['id']}")
        if not ok:
            failed += 1
            print(f"  expected={case['expect']}")
            print(f"  actual  ={out}")

    out_path = ROOT / "tests" / "shared" / "python_results.json"
    out_path.write_text(json.dumps(results, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"python: {len(results) - failed}/{len(results)} passed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
