"""SHA-256 manifest written after a bag is closed."""

from __future__ import annotations

import hashlib

from drone_logging.events import json_escape


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def manifest_sha256(files: dict[str, bytes]) -> str:
    lines = [f"{sha256_hex(files[name])}  {name}" for name in sorted(files)]
    return "\n".join(lines) + ("\n" if lines else "")


def manifest_meta(drone_id: str, software: str, start: str, end: str, files: dict[str, bytes]) -> str:
    entries = []
    for name in sorted(files):
        data = files[name]
        entries.append(
            '{"name":"%s","sha256":"%s","size":%d}' % (json_escape(name), sha256_hex(data), len(data))
        )
    body = (
        '{"drone_id":"%s","end":"%s","files":[%s],"software":"%s","start":"%s"}\n'
        % (json_escape(drone_id), json_escape(end), ",".join(entries), json_escape(software), json_escape(start))
    )
    return body


def verify_sha256_manifest(text: str, files: dict[str, bytes]) -> bool:
    listed = {}
    for line in text.splitlines():
        if not line.strip():
            continue
        digest, name = line.split("  ", 1)
        listed[name] = digest
    if set(listed) != set(files):
        return False
    return all(listed[name] == sha256_hex(files[name]) for name in listed)
