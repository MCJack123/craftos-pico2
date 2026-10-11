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
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DPICO_MALLOC_PANIC=0")
set(CMAKE_C_FLAGS_DEBUG "-g -O0")

# Initialize the Pico SDK
pico_sdk_init()

include_directories(craftos2-pico
    src
    vendor/sparkfun-pico/sparkfun_pico
    vendor/littlefs
    vendor/freertos-fat/src/Lab-Project-FreeRTOS-FAT/include/
    vendor/freertos-fat/src/FreeRTOS+FAT+CLI/include/ 
    vendor/freertos-fat/src/FreeRTOS+FAT+CLI/portable/RP2040/
)
add_executable(craftos-pico2
    src/modules/terminal.cpp src/modules/fs.c src/modules/fs_lfs.c src/modules/fs_fat.c
    src/drivers/hid.cpp src/drivers/screen.cpp src/drivers/audio.c src/drivers/1wire.c
    src/peripheral/peripheral_detect.cpp
    src/peripheral/drive.cpp src/peripheral/speaker.cpp src/peripheral/modem.cpp
    src/machine.cpp src/event.cpp src/funcs.cpp
    src/craftos2_rom.c
    src/main.cpp

    vendor/littlefs/lfs.c vendor/littlefs/lfs_util.c
    vendor/freertos-fat/src/Lab-Project-FreeRTOS-FAT/ff_crc.c
    vendor/freertos-fat/src/Lab-Project-FreeRTOS-FAT/ff_dir.c
    vendor/freertos-fat/src/Lab-Project-FreeRTOS-FAT/ff_error.c
    vendor/freertos-fat/src/Lab-Project-FreeRTOS-FAT/ff_fat.c
    vendor/freertos-fat/src/Lab-Project-FreeRTOS-FAT/ff_file.c
    vendor/freertos-fat/src/Lab-Project-FreeRTOS-FAT/ff_format.c
    vendor/freertos-fat/src/Lab-Project-FreeRTOS-FAT/ff_ioman.c
    vendor/freertos-fat/src/Lab-Project-FreeRTOS-FAT/ff_locking.c
    vendor/freertos-fat/src/Lab-Project-FreeRTOS-FAT/ff_memory.c
    vendor/freertos-fat/src/Lab-Project-FreeRTOS-FAT/ff_stdio.c
    vendor/freertos-fat/src/Lab-Project-FreeRTOS-FAT/ff_string.c
    vendor/freertos-fat/src/Lab-Project-FreeRTOS-FAT/ff_sys.c
    vendor/freertos-fat/src/Lab-Project-FreeRTOS-FAT/ff_time.c 
    vendor/freertos-fat/src/FreeRTOS+FAT+CLI/portable/RP2040/dma_interrupts.c
    vendor/freertos-fat/src/FreeRTOS+FAT+CLI/portable/RP2040/ff_sddisk.c
    vendor/freertos-fat/src/FreeRTOS+FAT+CLI/portable/RP2040/sd_card.c
    vendor/freertos-fat/src/FreeRTOS+FAT+CLI/portable/RP2040/SPI/sd_card_spi.c
    vendor/freertos-fat/src/FreeRTOS+FAT+CLI/portable/RP2040/SPI/sd_spi.c
    vendor/freertos-fat/src/FreeRTOS+FAT+CLI/portable/RP2040/SPI/my_spi.c
    vendor/freertos-fat/src/FreeRTOS+FAT+CLI/portable/RP2040/SDIO/sd_card_sdio.c
    vendor/freertos-fat/src/FreeRTOS+FAT+CLI/portable/RP2040/SDIO/rp2040_sdio.c
    #vendor/freertos-fat/src/FreeRTOS+FAT+CLI/src/crash.c
    vendor/freertos-fat/src/FreeRTOS+FAT+CLI/src/crc.c
    vendor/freertos-fat/src/FreeRTOS+FAT+CLI/src/ff_utils.c
    vendor/freertos-fat/src/FreeRTOS+FAT+CLI/src/file_stream.c
    #vendor/freertos-fat/src/FreeRTOS+FAT+CLI/src/freertos_callbacks.c
    #vendor/freertos-fat/src/FreeRTOS+FAT+CLI/src/FreeRTOS_strerror.c
    #vendor/freertos-fat/src/FreeRTOS+FAT+CLI/src/FreeRTOS_time.c
    vendor/freertos-fat/src/FreeRTOS+FAT+CLI/src/my_debug.c
    vendor/freertos-fat/src/FreeRTOS+FAT+CLI/src/sd_timeouts.c
    vendor/freertos-fat/src/FreeRTOS+FAT+CLI/src/util.c
)
pico_generate_pio_header(craftos-pico2 ${CMAKE_CURRENT_LIST_DIR}/src/drivers/1wire.pio)

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
    pico_multicore pico_stdlib pico_platform pico_flash pico_time pico_aon_timer pico_sync
    hardware_pwm hardware_spi hardware_dma hardware_pio hardware_flash hardware_adc hardware_clocks hardware_timer
    st7789 rgbled pico_graphics
    FreeRTOS-Kernel FreeRTOS-Kernel-Heap4
    sparkfun_pico
    tinyusb_host
    craftos-base
    cmsis_core
)
pico_add_extra_outputs(craftos-pico2)
