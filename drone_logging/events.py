"""JSON-lines events for failsafe and mode transitions.

Unverified flight-safety logs are never deleted. Retained categories stay
on disk even after they are verified.
"""

from __future__ import annotations

import os


def json_escape(text: str) -> str:
    out = []
    for char in text:
        if char == '"':
            out.append('\\"')
        elif char == "\\":
            out.append("\\\\")
        elif char == "\n":
            out.append("\\n")
        elif char == "\r":
            out.append("\\r")
        elif char == "\t":
            out.append("\\t")
        elif ord(char) < 0x20:
            out.append("\\u%04x" % ord(char))
        else:
            out.append(char)
    return "".join(out)


def format_event(kind: str, t_mono_ns: int, t_wall: str, src: str, dst: str, reason: str) -> str:
    return (
        '{"kind":"%s","t_mono_ns":%d,"t_wall":"%s","from":"%s","to":"%s","reason":"%s"}'
        % (json_escape(kind), int(t_mono_ns), json_escape(t_wall), json_escape(src), json_escape(dst), json_escape(reason))
    )


def append_event(path: str, line: str) -> None:
    with open(path, "a", encoding="utf-8") as handle:
        handle.write(line)
        if not line.endswith("\n"):
            handle.write("\n")


def delete_decision(category: str, state: str, retained: list[str]) -> str:
    kept = set(retained)
    if category in kept and state != "VERIFIED":
        return "DELETE_REFUSED unverified_flight_safety"
    if state != "VERIFIED":
        return "DELETE_REFUSED unverified"
    if category in kept:
        return "DELETE_REFUSED retained_flight_safety"
    return "DELETE_OK"


def apply_delete(path: str, category: str, state: str, retained: list[str]) -> str:
    decision = delete_decision(category, state, retained)
    if decision == "DELETE_OK":
        os.remove(path)
    return decision
