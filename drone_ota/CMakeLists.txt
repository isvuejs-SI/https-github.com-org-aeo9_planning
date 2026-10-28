cmake_minimum_required(VERSION 3.16)
project(drone_ota CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenSSL REQUIRED)

add_library(drone_ota
  src/sha256.cpp
  src/ed25519.cpp
  src/gate.cpp
  src/manifest.cpp
  src/slots.cpp
  src/health.cpp
)
target_include_directories(drone_ota PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(drone_ota PUBLIC OpenSSL::Crypto)
target_compile_options(drone_ota PRIVATE -Wall -Wextra)

add_executable(drone_ota_cli src/cli.cpp)
target_link_libraries(drone_ota_cli PRIVATE drone_ota)
target_compile_options(drone_ota_cli PRIVATE -Wall -Wextra)

find_package(GTest REQUIRED)
include(GoogleTest)

add_executable(drone_ota_tests tests/test_ota.cpp)
target_link_libraries(drone_ota_tests PRIVATE drone_ota GTest::gtest_main)
target_compile_definitions(drone_ota_tests PRIVATE DRONE_OTA_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(drone_ota_tests PRIVATE -Wall -Wextra)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  enable_testing()
endif()
add_test(NAME drone_ota_tests COMMAND drone_ota_tests)

find_package(ament_cmake QUIET)
if(ament_cmake_FOUND AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  install(TARGETS drone_ota drone_ota_cli
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION lib/${PROJECT_NAME})
  install(DIRECTORY config DESTINATION share/${PROJECT_NAME})
  install(DIRECTORY include/ DESTINATION include)
  ament_package()
endif()
