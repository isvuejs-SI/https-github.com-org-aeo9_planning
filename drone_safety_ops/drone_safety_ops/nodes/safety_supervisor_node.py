"""ROS 2 entry point for the safety_supervisor verdict node."""

from drone_safety_ops.nodes.runner import main_role


def main():
    return main_role("safety_supervisor")


if __name__ == "__main__":
    raise SystemExit(main())
