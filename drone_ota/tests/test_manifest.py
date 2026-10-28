"""Signed mission and bundle checks. Keys exist only in memory."""

from pathlib import Path

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey

from drone_ota.manifest import ListedFile, canonical_manifest, sha256_hex, verify_bundle, verify_mission

ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT / "tests" / "fixtures"


def _sign(manifest: str):
    private = Ed25519PrivateKey.generate()
    signature = private.sign(manifest.encode("utf-8"))
    public = private.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)
    return public, signature


def test_canonical_bytes():
    text = canonical_manifest(
        "1.3.0",
        "jetson-orin",
        "1.0.0",
        "1.3.0",
        [ListedFile("a" * 64, "b.txt", 2), ListedFile("b" * 64, "a.txt", 1)],
    )
    assert text == (FIXTURES / "canonical.txt").read_text(encoding="utf-8")


def test_lesson_mission_cases():
    plan = (FIXTURES / "mission_v1.3.0.plan").read_bytes()
    tampered = (FIXTURES / "mission_tampered.plan").read_bytes()
    assert sha256_hex(plan)[:12] == "9b90f8973adf"
    manifest = sha256_hex(plan) + " mission_v1.3.0.plan 1.3.0\n"
    public, signature = _sign(manifest)
    files = {"mission_v1.3.0.plan": plan, "mission_tampered.plan": tampered, "other.plan": plan}
    assert verify_mission(manifest, signature, public, files, "mission_v1.3.0.plan", True, False) == (
        "ACCEPT ack version=1.3.0"
    )
    assert verify_mission(manifest, signature, public, files, "mission_tampered.plan", True, False) == (
        "REJECT hash_mismatch"
    )
    assert verify_mission(manifest, signature, public, files, "mission_v1.3.0.plan", False, True) == (
        "REJECT not_landed_or_armed"
    )
    assert verify_mission(manifest, signature, public, files, "other.plan", True, False) == (
        "REJECT name_or_version_not_pinned"
    )
    bad = bytearray(signature)
    bad[0] ^= 0x01
    assert verify_mission(manifest, bytes(bad), public, files, "mission_v1.3.0.plan", False, True) == (
        "REJECT bad_signature"
    )


def test_bundle_signature_then_hashes():
    body = b"abc"
    listed = ListedFile(sha256_hex(body), "plan.txt", len(body))
    text = canonical_manifest("1.4.0", "jetson-orin", "1.0.0", "2.0.0", [listed])
    public, signature = _sign(text)
    files = {"plan.txt": body}
    assert verify_bundle(text, signature, public, files, "jetson-orin", "1.3.0") == []

    bad = bytearray(signature)
    bad[0] ^= 0x01
    assert verify_bundle(text, bytes(bad), public, files, "jetson-orin", "1.3.0") == ["bad_signature"]
    assert verify_bundle(text, signature, public, {"plan.txt": b"abX"}, "jetson-orin", "1.3.0") == ["hash_mismatch"]

    old = canonical_manifest("1.2.0", "jetson-orin", "1.0.0", "2.0.0", [listed])
    _public, old_sig = _sign(old)
    assert verify_bundle(old, old_sig, _public, files, "jetson-orin", "1.3.0") == ["version_rollback"]

    wrong = canonical_manifest("1.4.0", "other-board", "1.0.0", "2.0.0", [listed])
    hw_pub, hw_sig = _sign(wrong)
    assert verify_bundle(wrong, hw_sig, hw_pub, files, "jetson-orin", "1.3.0") == ["wrong_hardware"]

    fresh = canonical_manifest("1.4.0", "jetson-orin", "1.0.0", "2.0.0", [listed])
    new_pub, new_sig = _sign(fresh)
    assert verify_bundle(fresh, new_sig, new_pub, files, "jetson-orin", "0.9.0") == ["incompatible_version"]


def test_no_private_key_material():
    marker = "BEGIN " + "PRIVATE KEY"
    root = ROOT.parent
    for path in root.rglob("*"):
        if not path.is_file():
            continue
        if any(part in {"build", ".git", "__pycache__"} for part in path.parts):
            continue
        if path.suffix in {".pyc", ".o", ".a", ".so"}:
            continue
        assert not path.name.endswith(".pem")
        blob = path.read_bytes()
        assert marker.encode("utf-8") not in blob
