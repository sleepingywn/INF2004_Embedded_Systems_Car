# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in the Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(ir_test C CXX ASM)

pico_sdk_init()

add_executable(ir_test ir_test.c)

target_compile_options(ir_test PRIVATE -Wall -Wextra)
target_link_libraries(ir_test pico_stdlib hardware_adc)

# printf goes over USB (virtual COM port), not UART
pico_enable_stdio_usb(ir_test 1)
pico_enable_stdio_uart(ir_test 0)

# produce ir_test.uf2 etc.
pico_add_extra_outputs(ir_test)
