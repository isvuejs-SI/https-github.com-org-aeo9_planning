from setuptools import setup

setup(
    name="drone_pc",
    version="0.1.0",
    description="Planning and control supervisor. PX4 owns failsafes.",
    packages=[
        "drone_pc",
        "drone_pc.common",
        "drone_pc.planning",
        "drone_pc.control",
        "drone_pc.safety",
        "drone_pc.tests",
    ],
    package_dir={
        "drone_pc": ".",
        "drone_pc.common": "common",
        "drone_pc.planning": "planning",
        "drone_pc.control": "control",
        "drone_pc.safety": "safety",
        "drone_pc.tests": "tests",
    },
    data_files=[
        ("share/ament_index/resource_index/packages", ["resource/drone_pc"]),
        ("share/drone_pc", ["package.xml"]),
        ("share/drone_pc/config", ["config/limits.yaml"]),
        (
            "share/drone_pc/launch",
            ["launch/drone_pc.launch.py", "launch/px4_sitl.launch.py"],
        ),
    ],
    entry_points={
        "console_scripts": [
            "planner_node = drone_pc.planning.planner_node:main",
            "offboard_node = drone_pc.control.offboard_node:main",
            "safety_node = drone_pc.safety.safety_node:main",
        ],
    },
    python_requires=">=3.10",
)
