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
# Official Raspberry Pi Pico W by default. To build for the older Pimoroni
# Pico Plus 2 W target, configure with:
#   cmake -B build -DPICO_BOARD=pimoroni_pico_plus2_w_rp2350
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(pov_leds C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(pov_leds
        pov_leds.cpp
        ws2812_driver.cpp
        pov_demo.cpp
        hall_sensor.cpp
        dhcpserver.c
        wifi_config/wifi_config.c
        wifi_config/wifi_flash.c
        wifi_config/wifi_scan.c
        wifi_config/wifi_web.c
        wifi_config/wifi_http.c
        wifi_config/wifi_dns.c
        wifi_config/wifi_sta_web.c
        wifi_config/wifi_sta_http.c
)

pico_set_program_name(pov_leds "pov_leds")
pico_set_program_version(pov_leds "0.1")

if(PICO_BOARD STREQUAL "pimoroni_pico_plus2_w_rp2350")
        # This RP2350B board wires the RM2 (CYW43439) wireless module to
        # non-standard GPIOs. Do not apply these overrides to the official
        # Pico W: its CYW43 pins are fixed by the SDK's pico_w board header.
        target_compile_definitions(pov_leds PRIVATE
                CYW43_DEFAULT_PIN_WL_REG_ON=36
                CYW43_DEFAULT_PIN_WL_DATA_OUT=37
                CYW43_DEFAULT_PIN_WL_DATA_IN=37
                CYW43_DEFAULT_PIN_WL_HOST_WAKE=37
                CYW43_DEFAULT_PIN_WL_CLOCK=39
                CYW43_DEFAULT_PIN_WL_CS=38
        )

        # feature 002: reserve the last 4 KB flash sector for WiFi credentials
        # on the Pimoroni RP2350B/16 MB layout, with a build-time ASSERT guard.
        # The official Pico W uses the SDK's RP2040 linker script instead.
        pico_set_linker_script(pov_leds ${CMAKE_CURRENT_LIST_DIR}/memmap_wifi_creds.ld)
endif()

# Generate PIO header
pico_generate_pio_header(pov_leds ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

# Development validation mode: USB stdio enabled for runtime logging.
# Release builds should set POV_DEMO_DEV_USB_STDIO=OFF.
option(POV_DEMO_DEV_USB_STDIO "Enable USB stdio for POV demo validation" ON)
pico_enable_stdio_uart(pov_leds 0)
if(POV_DEMO_DEV_USB_STDIO)
        pico_enable_stdio_usb(pov_leds 1)
else()
        pico_enable_stdio_usb(pov_leds 0)
endif()

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
        hardware_dma
        hardware_timer
        hardware_clocks
        hardware_flash
        hardware_watchdog
        hardware_gpio
        hardware_irq
        hardware_sync
        pico_cyw43_arch_lwip_poll
        )

pico_add_extra_outputs(pov_leds)

