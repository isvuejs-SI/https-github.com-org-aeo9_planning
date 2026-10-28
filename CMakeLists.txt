cmake_minimum_required(VERSION 3.16)
project(drone_devops CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

enable_testing()
add_subdirectory(drone_ota)
add_subdirectory(drone_logging)
add_subdirectory(drone_telemetry)
