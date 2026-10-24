"""AEO-1: prove repo skeleton / CI layout is loadable."""

from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


def test_required_directories_exist():
    for name in ("planner", "control", "sim", "tests", "config", "cpp"):
        assert (ROOT / name).is_dir(), f"missing directory: {name}"


def test_config_and_ci_scripts_exist():
    assert (ROOT / "config" / "aeo9.yaml").is_file()
    assert (ROOT / "run_ci.sh").is_file()
    assert (ROOT / ".gitignore").is_file()
    assert (ROOT / "CMakeLists.txt").is_file()


def test_gitignore_excludes_sensitive_paths():
    text = (ROOT / ".gitignore").read_text(encoding="utf-8")
    for needle in ("logs/", "keys/", "footage/", "*.secret", ".env"):
        assert needle in text


def test_packages_import():
    import control  # noqa: F401
    import planner  # noqa: F401
    import sim  # noqa: F401
