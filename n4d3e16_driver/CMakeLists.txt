cmake_minimum_required(VERSION 3.16)
project(n4d3e16_driver VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(n4d3e16
    src/modbus_rtu.cpp
    src/n4d3e16_driver.cpp
    src/io_controller.cpp)
target_include_directories(n4d3e16 PUBLIC include)
target_compile_options(n4d3e16 PRIVATE -Wall -Wextra -Wpedantic)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_sources(n4d3e16 PRIVATE src/serial_port.cpp)
    add_executable(amr_io src/main.cpp)
    target_link_libraries(amr_io PRIVATE n4d3e16)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(driver_tests tests/driver_tests.cpp)
    target_link_libraries(driver_tests PRIVATE n4d3e16)
    add_test(NAME driver_tests COMMAND driver_tests)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        find_package(Threads REQUIRED)
        add_executable(serial_pty_tests tests/serial_pty_tests.cpp)
        target_link_libraries(serial_pty_tests PRIVATE n4d3e16 Threads::Threads util)
        add_test(NAME serial_pty_tests COMMAND serial_pty_tests)
    endif()
endif()
