cmake_minimum_required(VERSION 3.16)
project(drone_safety_ops)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(safety_ops_table test/table_main.cpp)
target_include_directories(safety_ops_table PRIVATE include)

find_package(ament_cmake QUIET)
if(ament_cmake_FOUND)
  find_package(rclcpp REQUIRED)
  find_package(std_msgs REQUIRED)
  find_package(ament_cmake_python REQUIRED)

  set(NODE_ROLES supervisor mission_gate day3 guard landing route alert safety_supervisor)
  foreach(role ${NODE_ROLES})
    add_executable(${role}_node src/ros_main.cpp)
    target_compile_definitions(${role}_node PRIVATE DSO_ROLE="${role}")
    target_include_directories(${role}_node PRIVATE include)
    ament_target_dependencies(${role}_node rclcpp std_msgs)
    install(TARGETS ${role}_node DESTINATION lib/${PROJECT_NAME})
  endforeach()

  ament_python_install_package(${PROJECT_NAME})
  install(TARGETS safety_ops_table DESTINATION lib/${PROJECT_NAME})
  install(DIRECTORY include/ DESTINATION include)
  install(DIRECTORY launch config systemd DESTINATION share/${PROJECT_NAME})
  install(PROGRAMS
    scripts/supervisor_node_py
    scripts/mission_gate_node_py
    scripts/day3_node_py
    scripts/guard_node_py
    scripts/landing_node_py
    scripts/route_node_py
    scripts/alert_node_py
    scripts/safety_supervisor_node_py
    DESTINATION lib/${PROJECT_NAME})

  if(BUILD_TESTING)
    find_package(launch_testing_ament_cmake QUIET)
    if(launch_testing_ament_cmake_FOUND)
      add_launch_test(test/launch/test_safety_ops_launch.py)
    endif()
  endif()
  ament_package()
else()
  message(STATUS "ROS 2 ament was not found. Built the standalone verdict table only.")
endif()
