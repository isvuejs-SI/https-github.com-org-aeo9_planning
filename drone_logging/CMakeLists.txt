cmake_minimum_required(VERSION 3.16)
project(drone_logging CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT TARGET drone_ota)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../drone_ota ${CMAKE_BINARY_DIR}/drone_ota_dep)
endif()

add_library(drone_logging src/logging.cpp)
target_include_directories(drone_logging PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(drone_logging PUBLIC drone_ota)
target_compile_options(drone_logging PRIVATE -Wall -Wextra)

find_package(GTest REQUIRED)

add_executable(drone_logging_tests tests/test_logging.cpp)
target_link_libraries(drone_logging_tests PRIVATE drone_logging GTest::gtest_main)
target_compile_definitions(drone_logging_tests PRIVATE DRONE_LOGGING_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(drone_logging_tests PRIVATE -Wall -Wextra)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  enable_testing()
endif()
add_test(NAME drone_logging_tests COMMAND drone_logging_tests)

find_package(ament_cmake QUIET)
if(ament_cmake_FOUND AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  install(TARGETS drone_logging
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION lib/${PROJECT_NAME})
  install(DIRECTORY config launch DESTINATION share/${PROJECT_NAME})
  install(DIRECTORY include/ DESTINATION include)
  ament_package()
endif()
