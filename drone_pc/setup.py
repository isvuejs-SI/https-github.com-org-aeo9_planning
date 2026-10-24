from setuptools import setup

package_name = "drone_pc"

setup(
    name=package_name,
    version="0.1.0",
    packages=[
        package_name,
        f"{package_name}.planning",
        f"{package_name}.control",
        f"{package_name}.safety",
    ],
    data_files=[
        ("share/ament_index/resource_index/packages", [f"resource/{package_name}"]),
        (f"share/{package_name}", ["package.xml"]),
    ],
    install_requires=["setuptools"],
    zip_safe=True,
    maintainer="drone_pc",
    maintainer_email="ops@example.com",
    description="Planning & Control for autonomous solar-charging drone",
    license="Apache-2.0",
    tests_require=["pytest"],
    entry_points={
        "console_scripts": [
            "drone_pc_py = drone_pc.drone_pc_node:main",
        ],
    },
)
