"""A/B slot state for signed app bundles.

Write the idle slot, boot it, then commit or roll back. The previous slot
stays untouched until commit.
"""

from __future__ import annotations

from dataclasses import dataclass


@dataclass
class SlotState:
    active: str = "A"
    staged: str = ""
    pending: str = ""
    boot_failures: int = 0


def _idle(state: SlotState) -> str:
    return "B" if state.active == "A" else "A"


def install_idle(state: SlotState, reasons: list[str]) -> str:
    if reasons:
        return "DENY " + ",".join(reasons)
    state.staged = _idle(state)
    state.pending = ""
    return f"ALLOW wrote_slot={state.staged} keep_slot={state.active}"


def boot_staged(state: SlotState) -> str:
    if not state.staged:
        return "NO_STAGED"
    state.pending = state.staged
    state.boot_failures = 0
    return f"BOOT pending={state.pending} active={state.active}"


def commit_pending(state: SlotState) -> str:
    if not state.pending:
        return "NO_PENDING"
    state.active = state.pending
    state.pending = ""
    state.staged = ""
    state.boot_failures = 0
    return f"COMMIT active={state.active}"


def rollback_pending(state: SlotState) -> str:
    state.pending = ""
    state.staged = ""
    state.boot_failures = 0
    return f"ROLLBACK active={state.active}"


def note_health(state: SlotState, ok: bool, rollback_threshold: int) -> str:
    if ok:
        state.boot_failures = 0
        return "HEALTH_PASS"
    state.boot_failures += 1
    if state.boot_failures >= rollback_threshold:
        count = state.boot_failures
        active = state.active
        rollback_pending(state)
        return f"HEALTH_FAIL boot_loop={count} action=ROLLBACK active={active}"
    return (
        f"HEALTH_FAIL boot_loop={state.boot_failures} action=RETRY "
        f"pending={state.pending} active={state.active}"
    )
