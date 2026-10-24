#!/usr/bin/env python3
"""Assert C++ and Python shared-test outputs are identical per case id."""

from __future__ import annotations

import json
import sys
from pathlib import Path


def main() -> int:
    shared = Path(__file__).resolve().parent / "shared"
    py = json.loads((shared / "python_results.json").read_text(encoding="utf-8"))
    cpp = json.loads((shared / "cpp_results.json").read_text(encoding="utf-8"))
    py_by = {r["id"]: r for r in py}
    cpp_by = {r["id"]: r for r in cpp}
    ids = sorted(set(py_by) | set(cpp_by))
    failed = 0
    for cid in ids:
        if cid not in py_by or cid not in cpp_by:
            print(f"[FAIL] {cid}: missing in one language")
            failed += 1
            continue
        if not py_by[cid]["pass"] or not cpp_by[cid]["pass"]:
            print(f"[FAIL] {cid}: language-local failure py={py_by[cid]['pass']} cpp={cpp_by[cid]['pass']}")
            failed += 1
            continue
        if py_by[cid]["output"] != cpp_by[cid]["output"]:
            print(f"[FAIL] {cid}: output mismatch")
            print(f"  py ={py_by[cid]['output']}")
            print(f"  cpp={cpp_by[cid]['output']}")
            failed += 1
        else:
            print(f"[PASS] {cid}")
    print(f"compare: {len(ids) - failed}/{len(ids)} identical")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
