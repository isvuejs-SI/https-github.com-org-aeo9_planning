#!/usr/bin/env python3
"""Checks that do not need ROS 2 or PX4."""

import ast
import os
import subprocess
import unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), os.pardir))


def read(path):
    with open(path, encoding="utf-8") as handle:
        return handle.read()


class PackageTests(unittest.TestCase):
    def test_golden_contains_lockout_and_no_go(self):
        golden = read(os.path.join(ROOT, "test", "golden_table.txt"))
        self.assertIn("PROP_LOCKOUT", golden)
        self.assertIn("NO_GO", golden)
        self.assertIn("relayed", golden)
        self.assertIn("assumed", golden)
        self.assertIn("systemd_watchdog_sec 5 match", golden)
        self.assertNotIn("GO real flight", golden)

    def test_render_matches_golden(self):
        env = os.environ.copy()
        env["PYTHONPATH"] = ROOT
        out = subprocess.check_output(
            ["python3", "-m", "drone_safety_ops.table", "config/params.yaml", "systemd/drone-safety-ops.service"],
            cwd=ROOT,
            env=env,
            text=True,
        )
        golden = read(os.path.join(ROOT, "test", "golden_table.txt"))
        self.assertEqual(out, golden)

    def test_legal_facts_stay_in_the_readme(self):
        text = read(os.path.join(ROOT, "README.md"))
        for phrase in (
            "Grand Canyon NP bans launching, landing, operating drones without written superintendent approval",
            "2026 Superintendent's Compendium",
            "36 CFR 1.5",
            "Havasupai Reservation bans drones without written advance permission",
            "Tribal Chairperson or Council",
            "Tribe Code Chapter 18.8",
            "waiver of 107.31",
            "Remote ID required (Part 89)",
            "Part 108 is proposed, not in effect",
            "No real flights until every permission is in hand",
            "PX4 on the Pixhawk owns the final failsafes",
            "PROP_LOCKOUT",
        ):
            self.assertIn(phrase, text)

    def test_both_units_set_watchdog(self):
        for name in ("drone-safety-ops.service", "drone-safety-ops-cpp.service"):
            text = read(os.path.join(ROOT, "systemd", name))
            self.assertIn("Type=notify", text)
            self.assertIn("WatchdogSec=5", text)
            self.assertIn("NotifyAccess=main", text)

    def test_python_sources_parse(self):
        for dirpath, _dirs, files in os.walk(ROOT):
            if "build" in dirpath.split(os.sep):
                continue
            for name in files:
                if name.endswith(".py"):
                    path = os.path.join(dirpath, name)
                    ast.parse(read(path), filename=path)

    def test_node_reports_missing_ros(self):
        try:
            import rclpy  # noqa: F401
        except ImportError:
            rclpy = None
        if rclpy is not None:
            self.skipTest("rclpy is installed")
        env = os.environ.copy()
        env["PYTHONPATH"] = ROOT
        proc = subprocess.run(
            ["python3", "-m", "drone_safety_ops.nodes.safety_supervisor_node"],
            cwd=ROOT,
            env=env,
            text=True,
            capture_output=True,
        )
        self.assertEqual(proc.returncode, 2)
        self.assertIn("rclpy", proc.stdout + proc.stderr)

    def test_route_real_flight_stays_no_go_without_permission(self):
        from drone_safety_ops.params import load_params
        from drone_safety_ops.route import plan

        params = load_params(os.path.join(ROOT, "config", "params.yaml"))
        dist = params.route.straight_line_mi * params.route.mi_to_km
        verdict, _pack, _legs, _req = plan(dist, params.route, True, False, False, True)
        self.assertEqual(verdict, "NO_GO")
        verdict, _pack, _legs, _req = plan(dist, params.route, True, True, True, True)
        self.assertEqual(verdict, "NO_GO")
        verdict, _pack, _legs, _req = plan(dist, params.route, False, False, False, False)
        self.assertEqual(verdict, "NO_GO")


if __name__ == "__main__":
    unittest.main()
