cmake_minimum_required(VERSION 3.12)

# Pull in PICO SDK (must be before project)
include(pico_sdk_import.cmake)

# We also need PICO EXTRAS
#include(pico_extras_import.cmake)

# We also need Pimoroni
include(pimoroni_pico_import.cmake)

include(FreeRTOS_Kernel_import.cmake)

project(craftos-pico2 C CXX)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")

# Initialize the Pico SDK
pico_sdk_init()

include_directories(craftos2-pico src src/craftos2-lua/include sparkfun-pico/sparkfun_pico littlefs)
add_executable(craftos-pico2
    src/main.cpp
    src/modules/terminal.cpp
    src/modules/redstone.c
    src/modules/mmfs.c
    src/drivers/hid.cpp
    src/drivers/screen.cpp
    src/machine.cpp
    src/event.cpp
    src/craftos2_rom.c
    "src/craftos2-lua/src/lapi.c" "src/craftos2-lua/src/lauxlib.c" "src/craftos2-lua/src/lbaselib.c" "src/craftos2-lua/src/lbitlib.c" "src/craftos2-lua/src/lcode.c" "src/craftos2-lua/src/lcorolib.c"
    "src/craftos2-lua/src/lctype.c" "src/craftos2-lua/src/ldblib.c" "src/craftos2-lua/src/ldebug.c" "src/craftos2-lua/src/ldebug.c" "src/craftos2-lua/src/ldo.c" "src/craftos2-lua/src/ldump.c"
    "src/craftos2-lua/src/lfunc.c" "src/craftos2-lua/src/lgc.c" "src/craftos2-lua/src/linit.c" "src/craftos2-lua/src/liolib.c" "src/craftos2-lua/src/llex.c" "src/craftos2-lua/src/lmathlib.c"
    "src/craftos2-lua/src/lmem.c" "src/craftos2-lua/src/loadlib.c" "src/craftos2-lua/src/lobject.c" "src/craftos2-lua/src/lopcodes.c" "src/craftos2-lua/src/loslib.c" "src/craftos2-lua/src/lparser.c"
    "src/craftos2-lua/src/lstate.c" "src/craftos2-lua/src/lstring.c" "src/craftos2-lua/src/lstrlib.c" "src/craftos2-lua/src/ltable.c" "src/craftos2-lua/src/ltablib.c" "src/craftos2-lua/src/ltm.c"
    "src/craftos2-lua/src/lundump.c" "src/craftos2-lua/src/lutf8lib.c" "src/craftos2-lua/src/lvm.c" "src/craftos2-lua/src/lzio.c" "src/llock.c"
    "src/apis/fs_handle.c" "src/apis/fs.c" "src/apis/os.cpp" "src/apis/peripheral.c" "src/apis/redstone.c" "src/apis/term.cpp"
    littlefs/lfs.c littlefs/lfs_util.c
)

include(common/pimoroni_i2c)
include(common/pimoroni_bus)
include(drivers/st7789/st7789)
include(drivers/rgbled/rgbled)
include(libraries/pico_graphics/pico_graphics)
add_subdirectory(sparkfun-pico/sparkfun_pico)

pico_enable_stdio_usb(craftos-pico2 0)
pico_enable_stdio_uart(craftos-pico2 1)

target_link_libraries(craftos-pico2 PRIVATE
    pico_multicore pico_stdlib pico_platform pico_flash
    hardware_pwm hardware_spi hardware_dma hardware_pio hardware_flash
    st7789 rgbled pico_graphics
    FreeRTOS-Kernel FreeRTOS-Kernel-Heap4
    sparkfun_pico
    tinyusb_host
)
pico_add_extra_outputs(craftos-pico2)
