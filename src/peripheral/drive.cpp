#include "peripheral.hpp"
#include <FreeRTOS.h>
extern "C" {
#include <ff_headers.h>
#include <ff_sddisk.h>
#include <ff_stdio.h>
#include <ff_utils.h>
}
#include "../modules/fs.h"
#include "../event.hpp"
#include "hw_config.h"

extern "C" {
void my_assert_func(const char *file, int line, const char *func, const char *pred) {panic("assert fail: %s:%d<%s>: %s", file, line, func, pred);}
extern craftos_machine_t machine;
}

static struct disk_info {
    TaskHandle_t task;
    struct mount * mount_handle;
} disks[2] = {{NULL, &mounts[0]}, {NULL, &mounts[1]}};

/* Configuration of RP2040 hardware SPI object */
static spi_t spi_left = {
    .hw_inst = spi1,  // RP2040 SPI component
    .miso_gpio = 8,
    .mosi_gpio = 11,
    .sck_gpio = 10,    // GPIO number (not Pico pin number)
    .baud_rate = 62500000,  // Actual frequency: 10416666.
    .DMA_IRQ_num = DMA_IRQ_1
};

SemaphoreHandle_t * spi1_mutex = &spi_left.mutex;

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
    .mount_point = "A:",
    .type = SD_IF_SPI,
    .spi_if_p = &spi_if_left  // Pointer to the SPI interface driving this card
};

/* Configuration of RP2040 hardware SPI object */
static spi_t spi_right = {
    .hw_inst = spi0,  // RP2040 SPI component
    .miso_gpio = 32,
    .mosi_gpio = 35,
    .sck_gpio = 34,    // GPIO number (not Pico pin number)
    .baud_rate = 62500000,  // Actual frequency: 10416666.
    .DMA_IRQ_num = DMA_IRQ_0
};

SemaphoreHandle_t * spi0_mutex = &spi_right.mutex;

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
    .mount_point = "B:",
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

static int drive_isDiskPresent(lua_State *L) {
    struct mount * m = ((disk_info*)lua_touserdata(L, 1))->mount_handle;
    lua_pushboolean(L, m->type == FS_MOUNT_TYPE_FAT_SD);
    return 1;
}

static int drive_getDiskLabel(lua_State *L) {
    struct mount * m = ((disk_info*)lua_touserdata(L, 1))->mount_handle;
    if (m->type != FS_MOUNT_TYPE_FAT_SD) return 0;
    const char * label = m->fat.disk->pxIOManager->xPartition.pcVolumeLabel;
    int len = 11;
    while (len > 0 && (label[len-1] == ' ' || label[len-1] == 0)) len--;
    lua_pushlstring(L, m->fat.disk->pxIOManager->xPartition.pcVolumeLabel, len);
    return 1;
}

static int drive_setDiskLabel(lua_State *L) {
    struct mount * m = ((disk_info*)lua_touserdata(L, 1))->mount_handle;
    lua_remove(L, 1);
    if (m->type != FS_MOUNT_TYPE_FAT_SD) return luaL_error(L, "no disk inserted");
    /* TODO: this doesn't write to disk */
    strncpy(m->fat.disk->pxIOManager->xPartition.pcVolumeLabel, luaL_checkstring(L, 1), 11);
    return 0;
}

static int drive_hasData(lua_State *L) {
    struct mount * m = ((disk_info*)lua_touserdata(L, 1))->mount_handle;
    lua_pushboolean(L, m->type == FS_MOUNT_TYPE_FAT_SD);
    return 1;
}

static int drive_getMountPath(lua_State *L) {
    struct mount * m = ((disk_info*)lua_touserdata(L, 1))->mount_handle;
    if (m->type != FS_MOUNT_TYPE_FAT_SD) return 0;
    lua_pushstring(L, m == mounts ? "disk" : "disk2");
    return 1;
}

static int drive_hasAudio(lua_State *L) {
    lua_pushboolean(L, false);
    return 1;
}

static int drive_getAudioTitle(lua_State *L) {
    return 0;
}

static int drive_playAudio(lua_State *L) {
    return 0;
}

static int drive_stopAudio(lua_State *L) {
    return 0;
}

static int drive_getDiskID(lua_State *L) {
    struct mount * m = ((disk_info*)lua_touserdata(L, 1))->mount_handle;
    if (m->type != FS_MOUNT_TYPE_FAT_SD) return 0;
    lua_pushinteger(L, *(uint32_t*)&((sd_card_t*)m->fat.disk->pvTag)->state.CID[3]);
    return 1;
}

static bool drive_mount(bool right) {
    // why does the driver reset SS on deinit but not set it up on init?
    sd_spi_if_t * sd = right ? &spi_if_right : &spi_if_left;
    gpio_init(sd->ss_gpio);
    gpio_put(sd->ss_gpio, 1);  // Avoid any glitches when enabling output
    gpio_set_dir(sd->ss_gpio, GPIO_OUT);
    gpio_put(sd->ss_gpio, 1);

    FF_Disk_t *pxDisk = FF_SDDiskInit(right ? "B:" : "A:");
    if (pxDisk == NULL) return false;
    FF_Error_t xError = FF_SDDiskMount(pxDisk);
    if (FF_isERR(xError) != pdFALSE) {
        FF_PRINTF("FF_SDDiskMount: %s\n",
                  (const char *)FF_GetErrMessage(xError));
        FF_SDDiskDelete(pxDisk);
        return false;
    }
    mounts[right ? 1 : 0].type = FS_MOUNT_TYPE_FAT_SD;
    mounts[right ? 1 : 0].impl = &fs_fat;
    mounts[right ? 1 : 0].fat.disk = pxDisk;
    FF_FS_Add(right ? "B:" : "A:", pxDisk);
    craftos_machine_mount_real(machine, right ? "B:/" : "A:/", right ? "/disk2" : "/disk", peripheral_detect_query(right, PERIPHERAL_CMD_GET_PORT) & 0x10);
    event_t event;
    event.type = EVENT_TYPE_DISK;
    event.peripheral.side = right ? "right" : "left";
    event_push(&event);
    return true;
}

static void drive_unmount(bool right) {
    if (mounts[right ? 1 : 0].type == FS_MOUNT_TYPE_NONE) return;
    craftos_machine_unmount(machine, right ? "/disk2" : "/disk");
    FF_FS_Remove(right ? "B:" : "A:");
    mounts[right ? 1 : 0].type = FS_MOUNT_TYPE_NONE;
    mounts[right ? 1 : 0].impl = NULL;
    FF_Unmount(mounts[right ? 1 : 0].fat.disk);
    FF_SDDiskDelete(mounts[right ? 1 : 0].fat.disk);
    event_t event;
    event.type = EVENT_TYPE_DISK_EJECT;
    event.peripheral.side = right ? "right" : "left";
    event_push(&event);
}

static int drive_ejectDisk(lua_State *L) {
    struct mount * m = ((disk_info*)lua_touserdata(L, 1))->mount_handle;
    if (m->type != FS_MOUNT_TYPE_FAT_SD) return 0;
    drive_unmount(m != mounts);
    return 0;
}

static void drive_poll(void* isRight) {
    bool isMounted = false, mountSuccess = true;
    while (true) {
        if (ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(100))) break;
        int port = peripheral_detect_query(isRight != NULL, PERIPHERAL_CMD_GET_PORT);
        if (port != -1) {
            bool detect = port & 0x20;
            if (detect && !isMounted) {
                vTaskDelay(pdMS_TO_TICKS(1000)); // wait for SD card to be fully inserted/debounce
                mountSuccess = drive_mount(isRight != NULL);
            } else if (!detect && isMounted && mountSuccess) drive_unmount(isRight != NULL);
            isMounted = detect;
        }
    }
    if (isMounted) drive_unmount(isRight != NULL);
    sd_spi_if_t * sd = isRight ? &spi_if_right : &spi_if_left;
    gpio_set_function(sd->spi->miso_gpio, GPIO_FUNC_NULL);
    gpio_set_function(sd->spi->mosi_gpio, GPIO_FUNC_NULL);
    gpio_set_function(sd->spi->sck_gpio, GPIO_FUNC_NULL);
    gpio_set_function(sd->ss_gpio, GPIO_FUNC_NULL);
    vTaskDelete(NULL);
}

static int drive_open(bool right, void** userp_out) {
    sd_spi_if_t * sd = right ? &spi_if_right : &spi_if_left;
    gpio_set_function(sd->spi->miso_gpio, GPIO_FUNC_SPI);
    gpio_set_function(sd->spi->mosi_gpio, GPIO_FUNC_SPI);
    gpio_set_function(sd->spi->sck_gpio, GPIO_FUNC_SPI);
    gpio_init(sd->ss_gpio);
    gpio_put(sd->ss_gpio, 1);  // Avoid any glitches when enabling output
    gpio_set_dir(sd->ss_gpio, GPIO_OUT);
    gpio_put(sd->ss_gpio, 1);
    xTaskCreate(drive_poll, "drive poll", 1536, right ? (void*)1 : NULL, 3, &disks[right ? 1 : 0].task);
    *userp_out = &disks[right ? 1 : 0];
    return 0;
}

static void drive_close(void* userp) {
    xTaskNotify(((disk_info*)userp)->task, 1, eIncrement);
}

static const luaL_Reg peripheral_methods[] = {
    {"isDiskPresent", drive_isDiskPresent},
    {"getDiskLabel", drive_getDiskLabel},
    {"setDiskLabel", drive_setDiskLabel},
    {"hasData", drive_hasData},
    {"getMountPath", drive_getMountPath},
    {"hasAudio", drive_hasAudio},
    {"getAudioTitle", drive_getAudioTitle},
    {"playAudio", drive_playAudio},
    {"stopAudio", drive_stopAudio},
    {"ejectDisk", drive_ejectDisk},
    {"getDiskID", drive_getDiskID},
    {NULL, NULL}
};

struct peripheral peripheral_drive = {
    {"drive", NULL},
    peripheral_methods,
    drive_open,
    drive_close
};
