cmake_minimum_required(VERSION 3.13)

# The architecture of the Pico 2 (RP2350) is being forced
set(PICO_BOARD pico2)

include(pico_sdk_import.cmake)

project(pico_execution_engine C CXX ASM)
set(CMAKE_CXX_STANDARD 11)

pico_sdk_init()

add_executable(pico_engine
    main.cpp
)

# Libraries
target_link_libraries(pico_engine pico_stdlib)

# Input/output configuration: All via USB
pico_enable_stdio_usb(pico_engine 1)
pico_enable_stdio_uart(pico_engine 0)

pico_add_extra_outputs(pico_engine)
