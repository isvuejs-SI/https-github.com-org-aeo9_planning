"""ROS 2 entry point for the guard verdict node."""

from drone_safety_ops.nodes.runner import main_role


def main():
    return main_role("guard")


if __name__ == "__main__":
    raise SystemExit(main())
