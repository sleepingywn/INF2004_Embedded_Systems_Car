# Standalone GY-511 bench test (plain Pico SDK, no RTOS) -- NOT part of the
# team firmware.  Build it on its own:
#   mkdir build && cd build && cmake .. && make      (-DPICO_BOARD=pico2 for a Pico 2)
# It uses the SAME imu_logic.c as the firmware (app_program/subsystem4/),
# but its own Pico SDK version of drv_gy511.c in this folder.
cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED PICO_BOARD)
    set(PICO_BOARD pico)
endif()

include(pico_sdk_import.cmake)
project(gy511_bench C CXX ASM)
set(CMAKE_C_STANDARD 11)
pico_sdk_init()

set(LOGIC_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../app_program/subsystem4)

add_executable(gy511_bench
    bench_main.c
    drv_gy511.c                     # Pico SDK version (this folder)
    ${LOGIC_DIR}/imu_logic.c        # shared with the firmware
)
target_include_directories(gy511_bench PRIVATE ${LOGIC_DIR})
target_link_libraries(gy511_bench pico_stdlib hardware_i2c)
target_compile_options(gy511_bench PRIVATE -Wall -Wextra)

pico_enable_stdio_usb(gy511_bench 1)
pico_enable_stdio_uart(gy511_bench 0)
pico_add_extra_outputs(gy511_bench)
