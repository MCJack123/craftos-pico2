#include "peripheral.hpp"
#include <FreeRTOS.h>
#include <ff_headers.h>
#include <ff_sddisk.h>
#include <ff_utils.h>
#include "../modules/fs.h"
#include "hw_config.h"

/* Configuration of RP2040 hardware SPI object */
static spi_t spi_left = {
    .hw_inst = spi1,  // RP2040 SPI component
    .miso_gpio = 8,
    .mosi_gpio = 11,
    .sck_gpio = 10,    // GPIO number (not Pico pin number)
    .baud_rate = 12 * 1000 * 1000  // Actual frequency: 10416666.
};

/* SPI Interface */
static sd_spi_if_t spi_if_left = {
    .spi = &spi_left,  // Pointer to the SPI driving this card
    .ss_gpio = 9  // The SPI slave select GPIO for this SD card
};

/* Configuration of the SD Card socket object */
static sd_card_t sd_card_left = {
    // "device_name" is arbitrary:
    .device_name = "A:",
    // "mount_point" must be a directory off the file system's root directory and must be an absolute path:
    .mount_point = "/A:",
    .type = SD_IF_SPI,
    .spi_if_p = &spi_if_left  // Pointer to the SPI interface driving this card
};

/* Configuration of RP2040 hardware SPI object */
static spi_t spi_right = {
    .hw_inst = spi0,  // RP2040 SPI component
    .miso_gpio = 32,
    .mosi_gpio = 35,
    .sck_gpio = 34,    // GPIO number (not Pico pin number)
    .baud_rate = 12 * 1000 * 1000  // Actual frequency: 10416666.
};

/* SPI Interface */
static sd_spi_if_t spi_if_right = {
    .spi = &spi_right,  // Pointer to the SPI driving this card
    .ss_gpio = 33  // The SPI slave select GPIO for this SD card
};

/* Configuration of the SD Card socket object */
static sd_card_t sd_card_right = {
    // "device_name" is arbitrary:
    .device_name = "B:",
    // "mount_point" must be a directory off the file system's root directory and must be an absolute path:
    .mount_point = "/B:",
    .type = SD_IF_SPI,
    .spi_if_p = &spi_if_right  // Pointer to the SPI interface driving this card
};

/* ********************************************************************** */

size_t sd_get_num() { return 2; }

sd_card_t *sd_get_by_num(size_t num) {
    switch (num) {
        case 0: return &sd_card_left;
        case 1: return &sd_card_right;
        default: return NULL;
    }
}

static int drive_open(bool right, void** userp_out) {

    return -1;
}

static const luaL_Reg peripheral_methods[] = {
    {NULL, NULL}
};

struct peripheral peripheral_drive = {
    {"drive", NULL},
    peripheral_methods,
    drive_open,
    NULL
};