"""Ed25519 over the canonical manifest, then SHA-256 of each listed file."""

from __future__ import annotations

import hashlib
from dataclasses import dataclass

from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey


@dataclass
class ListedFile:
    sha256: str
    path: str
    size: int


def sha256_hex(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def canonical_manifest(
    version: str,
    hardware: str,
    min_compatible: str,
    max_compatible: str,
    files: list[ListedFile],
) -> str:
    ordered = sorted(files, key=lambda item: item.path)
    lines = [
        "DRONEOTA1",
        f"version={version}",
        f"hardware={hardware}",
        f"min_compatible={min_compatible}",
        f"max_compatible={max_compatible}",
    ]
    for item in ordered:
        lines.append(f"{item.sha256}  {item.size} {item.path}")
    return "\n".join(lines) + "\n"


def _version_key(text: str) -> tuple[int, ...]:
    return tuple(int(part) for part in text.split("."))


def _cmp_version(left: str, right: str) -> int:
    a = list(_version_key(left))
    b = list(_version_key(right))
    width = max(len(a), len(b))
    a.extend([0] * (width - len(a)))
    b.extend([0] * (width - len(b)))
    if a < b:
        return -1
    if a > b:
        return 1
    return 0


def parse_bundle_manifest(text: str) -> dict:
    lines = text.splitlines()
    if not lines or lines[0] != "DRONEOTA1":
        raise ValueError("not a bundle manifest")
    info = {"version": "", "hardware": "", "min_compatible": "", "max_compatible": "", "files": []}
    for line in lines[1:]:
        if not line:
            continue
        if "=" in line and (line.find("  ") < 0 or line.find("=") < line.find("  ")):
            key, value = line.split("=", 1)
            if key not in info:
                raise ValueError("unknown manifest key")
            info[key] = value
            continue
        digest, size, path = line.split()
        info["files"].append(ListedFile(digest, path, int(size)))
    return info


def parse_lesson_manifest(text: str) -> list[tuple[str, str, str]]:
    entries = []
    for line in text.splitlines():
        if not line.strip():
            continue
        digest, name, version = line.split()
        entries.append((digest, name, version))
    return entries


def _verify(public_key: bytes, message: bytes, signature: bytes) -> bool:
    if len(public_key) != 32 or len(signature) != 64:
        return False
    try:
        Ed25519PublicKey.from_public_bytes(public_key).verify(signature, message)
        return True
    except Exception:
        return False


def verify_mission(
    manifest: str,
    signature: bytes,
    public_key: bytes,
    files: dict[str, bytes],
    requested: str,
    landed: bool,
    armed: bool,
) -> str:
    if not _verify(public_key, manifest.encode("utf-8"), signature):
        return "REJECT bad_signature"
    if armed or not landed:
        return "REJECT not_landed_or_armed"
    entries = parse_lesson_manifest(manifest)
    if not entries:
        return "REJECT hash_mismatch"
    want_hash, want_name, want_ver = entries[0]
    data = files.get(requested)
    if data is None or sha256_hex(data) != want_hash:
        return "REJECT hash_mismatch"
    if requested != want_name:
        return "REJECT name_or_version_not_pinned"
    return "ACCEPT ack version=" + want_ver


def verify_bundle(
    manifest: str,
    signature: bytes,
    public_key: bytes,
    files: dict[str, bytes],
    expected_hardware: str,
    installed_version: str,
) -> list[str]:
    if not _verify(public_key, manifest.encode("utf-8"), signature):
        return ["bad_signature"]
    info = parse_bundle_manifest(manifest)
    reasons: list[str] = []
    hash_bad = False
    for listed in info["files"]:
        data = files.get(listed.path)
        if data is None or len(data) != listed.size or sha256_hex(data) != listed.sha256:
            hash_bad = True
    if hash_bad:
        reasons.append("hash_mismatch")
    if info["hardware"] != expected_hardware:
        reasons.append("wrong_hardware")
    if _cmp_version(installed_version, info["min_compatible"]) < 0 or _cmp_version(
        installed_version, info["max_compatible"]
    ) > 0:
        reasons.append("incompatible_version")
    if _cmp_version(info["version"], installed_version) <= 0:
        reasons.append("version_rollback")
    return reasons
