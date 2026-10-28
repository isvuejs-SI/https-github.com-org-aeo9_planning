"""Python signatures must verify in the C++ CLI, and the reverse."""

import os
import subprocess
from pathlib import Path

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey, Ed25519PublicKey

ROOT = Path(__file__).resolve().parents[2]


def _cli() -> Path:
    env = os.environ.get("DRONE_OTA_CLI")
    path = Path(env) if env else ROOT / "build" / "drone_ota" / "drone_ota_cli"
    assert path.is_file(), "build drone_ota_cli before the cross-language signature test"
    return path


def test_cpp_verifies_python_signature(tmp_path):
    message = tmp_path / "manifest.txt"
    message.write_bytes(b"DRONEOTA1\nversion=1.3.0\n")
    private = Ed25519PrivateKey.generate()
    signature = private.sign(message.read_bytes())
    public = private.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
    result = subprocess.run(
        [_cli(), "verify-sig", public.hex(), signature.hex(), str(message)],
        check=True,
        capture_output=True,
        text=True,
    )
    assert result.stdout == "VERIFY_OK\n"
    tampered = tmp_path / "tampered.txt"
    tampered.write_bytes(b"DRONEOTA1\nversion=9.9.9\n")
    rejected = subprocess.run(
        [_cli(), "verify-sig", public.hex(), signature.hex(), str(tampered)],
        check=True,
        capture_output=True,
        text=True,
    )
    assert rejected.stdout == "VERIFY_FAIL\n"


def test_python_verifies_cpp_signature(tmp_path):
    message = tmp_path / "manifest.txt"
    message.write_bytes(b"canonical manifest bytes\n")
    result = subprocess.run([_cli(), "sign-message", str(message)], check=True, capture_output=True, text=True)
    assert "PRIV " not in result.stdout
    lines = dict(line.split(" ", 1) for line in result.stdout.splitlines())
    public = bytes.fromhex(lines["PUB"])
    signature = bytes.fromhex(lines["SIG"])
    Ed25519PublicKey.from_public_bytes(public).verify(signature, message.read_bytes())
    try:
        Ed25519PublicKey.from_public_bytes(public).verify(signature, b"other")
    except Exception:
        return
    raise AssertionError("tampered message was accepted")
