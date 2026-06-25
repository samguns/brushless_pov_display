# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(pov_leds C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(pov_leds
        pov_leds.cpp
        dhcpserver.c
        wifi_config/wifi_config.c
        wifi_config/wifi_flash.c
        wifi_config/wifi_scan.c
        wifi_config/wifi_web.c
        wifi_config/wifi_http.c
        wifi_config/wifi_dns.c
)

pico_set_program_name(pov_leds "pov_leds")
pico_set_program_version(pov_leds "0.1")

# Generate PIO header
pico_generate_pio_header(pov_leds ${CMAKE_CURRENT_LIST_DIR}/blink.pio)

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(pov_leds 1)
pico_enable_stdio_usb(pov_leds 0)

# Add the standard library to the build
target_link_libraries(pov_leds
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(pov_leds PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/wifi_config
)

# Add any user requested libraries
target_link_libraries(pov_leds 
        hardware_pio
        hardware_timer
        hardware_clocks
        hardware_flash
        hardware_watchdog
        pico_cyw43_arch_lwip_poll
        )

pico_add_extra_outputs(pov_leds)

