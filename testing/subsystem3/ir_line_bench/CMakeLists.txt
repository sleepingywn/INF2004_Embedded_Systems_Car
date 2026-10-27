cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(PICO_BOARD pico_w CACHE STRING "Board type")

include(${CMAKE_CURRENT_LIST_DIR}/../ir_bench_test/pico_sdk_import.cmake)

project(ir_line_bench C CXX ASM)
pico_sdk_init()

# The S3 logic under test lives in the real firmware folder
set(S3_DIR ${CMAKE_CURRENT_LIST_DIR}/../../../app_program/subsystem3)

add_executable(ir_line_bench
        ir_line_bench.c
        ${S3_DIR}/sensor_line.c
        ${S3_DIR}/barcode_sense.c
        )
target_include_directories(ir_line_bench PRIVATE ${S3_DIR})
target_compile_options(ir_line_bench PRIVATE -Wall -Wextra)
target_link_libraries(ir_line_bench pico_stdlib hardware_adc hardware_sync)

pico_enable_stdio_usb(ir_line_bench 1)
pico_enable_stdio_uart(ir_line_bench 0)
pico_add_extra_outputs(ir_line_bench)
