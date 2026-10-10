#include "peripheral.hpp"
#include <FreeRTOS.h>
extern "C" {
#include <ff_headers.h>
#include <ff_sddisk.h>
#include <ff_stdio.h>
#include <ff_utils.h>
}
#include "../modules/fs.h"
#include "hw_config.h"

extern "C" {
void my_assert_func(const char *file, int line, const char *func, const char *pred) {}
}

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

static void drive_mount(bool right) {
    FF_Disk_t *pxDisk = FF_SDDiskInit(right ? "B:" : "A:");
    if (pxDisk == NULL) return;
    FF_Error_t xError = FF_SDDiskMount(pxDisk);
    if (FF_isERR(xError) != pdFALSE) {
        FF_PRINTF("FF_SDDiskMount: %s\n",
                  (const char *)FF_GetErrMessage(xError));
        return;
    }
    mounts[right ? 1 : 0].type = FS_MOUNT_TYPE_FAT_SD;
    mounts[right ? 1 : 0].fat.disk = pxDisk;
    FF_FS_Add(right ? "/B:" : "/A:", pxDisk);
}

static void drive_unmount(bool right) {
    FF_FS_Remove(right ? "/B:" : "/A:");
    FF_Unmount(mounts[right ? 1 : 0].fat.disk);
    FF_SDDiskDelete(mounts[right ? 1 : 0].fat.disk);
}

static void drive_poll(void* isRight) {
    bool isMounted = false;
    while (true) {
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100))) break;
        if (xSemaphoreTake(onewire_lock[isRight ? 1 : 0], pdMS_TO_TICKS(50)) == pdFALSE) continue;
        if (ow_reset(&onewire_inst[isRight ? 1 : 0])) {
            ow_send(&onewire_inst[isRight ? 1 : 0], PERIPHERAL_CMD_GET_PORT);
            bool detect = ow_read(&onewire_inst[isRight ? 1 : 0]) & 0x20;
            if (detect && !isMounted) drive_mount(isRight != NULL);
            else if (!detect && isMounted) drive_unmount(isRight != NULL);
            isMounted = detect;
        }
        xSemaphoreGive(onewire_lock[isRight ? 1 : 0]);
    }
    if (isMounted) drive_unmount(isRight != NULL);
    vTaskDelete(NULL);
}

static int drive_open(bool right, void** userp_out) {
    xTaskCreate(drive_poll, "drive poll", 1536, right ? (void*)1 : NULL, 2, (TaskHandle_t*)userp_out);
    return 0;
}

static void drive_close(void* userp) {
    xTaskNotify((TaskHandle_t)userp, 1, eIncrement);
}

static const luaL_Reg peripheral_methods[] = {
    {NULL, NULL}
};

struct peripheral peripheral_drive = {
    {"drive", NULL},
    peripheral_methods,
    drive_open,
    drive_close
};