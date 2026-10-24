cmake_minimum_required(VERSION 3.8)
project(drone_pc)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ament_cmake REQUIRED)
find_package(ament_cmake_python REQUIRED)

# Optional ROS deps — core library builds without them when STANDALONE=ON
option(DRONE_PC_STANDALONE "Build core lib and tests without ROS" OFF)

set(DRONE_PC_CORE_SOURCES
  src/planning/state_machine.cpp
  src/planning/occupancy_map.cpp
  src/planning/global_planner.cpp
  src/planning/local_planner.cpp
  src/planning/energy_planner.cpp
  src/planning/escape_planner.cpp
  src/control/px4_interface.cpp
  src/control/link_loss.cpp
  src/safety/preflight_gate.cpp
  src/safety/alerts.cpp
  src/safety/event_logger.cpp
  src/params.cpp
)

add_library(drone_pc_core ${DRONE_PC_CORE_SOURCES})
target_include_directories(drone_pc_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

if(NOT DRONE_PC_STANDALONE)
  find_package(rclcpp REQUIRED)
  find_package(std_msgs REQUIRED)
  find_package(geometry_msgs REQUIRED)
  find_package(nav_msgs REQUIRED)
  find_package(sensor_msgs REQUIRED)

  add_executable(drone_pc_node src/nodes/drone_pc_node.cpp)
  target_link_libraries(drone_pc_node drone_pc_core)
  ament_target_dependencies(drone_pc_node
    rclcpp std_msgs geometry_msgs nav_msgs sensor_msgs)

  install(TARGETS drone_pc_node
    DESTINATION lib/${PROJECT_NAME})
endif()

ament_python_install_package(${PROJECT_NAME})

install(TARGETS drone_pc_core
  EXPORT export_${PROJECT_NAME}
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)

install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY config launch DESTINATION share/${PROJECT_NAME})
install(DIRECTORY tests/shared DESTINATION share/${PROJECT_NAME}/tests)

if(BUILD_TESTING)
  # Standalone dual-language test harness (no gtest required)
  add_executable(drone_pc_cpp_tests tests/cpp/run_shared_tests.cpp)
  target_link_libraries(drone_pc_cpp_tests drone_pc_core)
  target_compile_definitions(drone_pc_cpp_tests PRIVATE
    DRONE_PC_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/shared")
endif()

ament_package()
