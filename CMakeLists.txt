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
set(PICO_BOARD pimoroni_pico_plus2_rp2350 CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.12)

# Pull in PICO SDK (must be before project)
include(pico_sdk_import.cmake)

# We also need PICO EXTRAS
#include(pico_extras_import.cmake)

# We also need Pimoroni
include(pimoroni_pico_import.cmake)

set(FREERTOS_KERNEL_PATH "${CMAKE_SOURCE_DIR}/vendor/FreeRTOS-Kernel")
include(FreeRTOS_Kernel_import.cmake)

project(craftos-pico2 C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")

# Initialize the Pico SDK
pico_sdk_init()

include_directories(craftos2-pico src vendor/sparkfun-pico/sparkfun_pico vendor/littlefs)
add_executable(craftos-pico2
    src/main.cpp
    src/modules/terminal.cpp
    src/drivers/hid.cpp
    src/drivers/screen.cpp
    src/peripheral/peripheral_detect.cpp
    src/machine.cpp
    src/event.cpp
    src/craftos2_rom.c
    src/funcs.cpp
    src/fs.c
    vendor/littlefs/lfs.c vendor/littlefs/lfs_util.c
)
pico_generate_pio_header(craftos-pico2 ${CMAKE_CURRENT_LIST_DIR}/src/peripheral/peripheral_id.pio)

include(common/pimoroni_i2c)
include(common/pimoroni_bus)
include(drivers/st7789/st7789)
include(drivers/rgbled/rgbled)
include(libraries/pico_graphics/pico_graphics)
add_subdirectory(vendor/sparkfun-pico/sparkfun_pico)
add_subdirectory(vendor/craftos-base)

pico_enable_stdio_usb(craftos-pico2 0)
pico_enable_stdio_uart(craftos-pico2 1)
pico_set_printf_implementation(craftos-pico2 compiler)

target_link_libraries(craftos-pico2 PRIVATE
    pico_multicore pico_stdlib pico_platform pico_flash pico_time pico_aon_timer
    hardware_pwm hardware_spi hardware_dma hardware_pio hardware_flash
    st7789 rgbled pico_graphics
    FreeRTOS-Kernel FreeRTOS-Kernel-Heap4
    sparkfun_pico
    tinyusb_host
    craftos-base
)
pico_add_extra_outputs(craftos-pico2)
