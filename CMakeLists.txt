cmake_minimum_required(VERSION 3.16)
project(aeo9_planning LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(yaml-cpp REQUIRED)
enable_testing()
find_package(GTest REQUIRED)

add_library(aeo9_lib
  cpp/src/config.cpp
  cpp/src/mission.cpp
  cpp/src/go_nogo.cpp
  cpp/src/failsafe.cpp
  cpp/src/geofence.cpp
  cpp/src/animal_escape.cpp
  cpp/src/gear.cpp
  cpp/src/px4_adapter.cpp
  cpp/src/ros2_bridge.cpp
)
target_include_directories(aeo9_lib PUBLIC cpp/include)
target_link_libraries(aeo9_lib PUBLIC yaml-cpp)

add_executable(aeo9_dump cpp/src/dump_main.cpp)
target_link_libraries(aeo9_dump PRIVATE aeo9_lib)

add_executable(aeo9_cpp_tests
  tests/cpp/test_skeleton.cpp
  tests/cpp/test_config.cpp
  tests/cpp/test_energy.cpp
  tests/cpp/test_failsafe.cpp
  tests/cpp/test_geofence.cpp
  tests/cpp/test_scenarios.cpp
)
target_link_libraries(aeo9_cpp_tests PRIVATE aeo9_lib GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(aeo9_cpp_tests
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)
