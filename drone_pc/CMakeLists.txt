cmake_minimum_required(VERSION 3.16)
project(drone_pc CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(drone_pc_contract_tests tests/test_contract.cpp)
target_include_directories(drone_pc_contract_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

enable_testing()
add_test(
  NAME contract_cpp
  COMMAND drone_pc_contract_tests ${CMAKE_CURRENT_SOURCE_DIR}
)

find_package(ament_cmake QUIET)
if(ament_cmake_FOUND)
  install(TARGETS drone_pc_contract_tests DESTINATION lib/${PROJECT_NAME})
  install(DIRECTORY config launch DESTINATION share/${PROJECT_NAME})
  install(PROGRAMS
    scripts/planner_node
    scripts/offboard_node
    scripts/safety_node
    DESTINATION lib/${PROJECT_NAME})
  ament_package()
endif()
