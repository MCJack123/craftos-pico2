#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sfe_pico_alloc.h>
#include <sfe_psram.h>
#include <lfs.h>
#include <pico/flash.h>
#include <hardware/flash.h>
#include <sfe_pico_boards.h>

extern lfs_t mounts[3];

const void* FLASH_PARTITION = (const void*)0x10200000;
static bool psramActive = true;

static int flash_read(const struct lfs_config *c, lfs_block_t block,
            lfs_off_t off, void *buffer, lfs_size_t size) {
    memcpy(buffer, FLASH_PARTITION + block * c->block_size + off, size);
    return 0;
}

struct flash_prog {
    lfs_block_t block;
    lfs_off_t off;
    const void *buffer;
    lfs_size_t size;
};

static void _flash_prog(void* info) {
    struct flash_prog* prog = info;
    if (psramActive) {
        // flush XIP cache to PSRAM
        for (volatile uint8_t* cache = (volatile uint8_t*)0x18000001; cache < (volatile uint8_t*)(0x18000001 + 2048 * 8); cache += 8)
            *cache = 0;
        psramActive = false;
    }
    flash_range_program((prog->block + 512) * 4096 + prog->off, prog->buffer, prog->size);
}

static int flash_prog(const struct lfs_config *c, lfs_block_t block,
            lfs_off_t off, const void *buffer, lfs_size_t size) {
    struct flash_prog prog = {block, off, buffer, size};
    return flash_safe_execute(_flash_prog, &prog, 100);
}

static void _flash_erase(void* addr) {
    if (psramActive) {
        // flush XIP cache to PSRAM
        for (volatile uint8_t* cache = (volatile uint8_t*)0x18000001; cache < (volatile uint8_t*)(0x18000001 + 2048 * 8); cache += 8)
            *cache = 0;
        psramActive = false;
    }
    flash_range_erase((uint32_t)(addr + 512) * 4096, 1);
}

static int flash_erase(const struct lfs_config *c, lfs_block_t block) {
    return flash_safe_execute(_flash_erase, (void*)block, 100);
}

static int flash_sync(const struct lfs_config *c) {
    sfe_setup_psram(SFE_RP2350_XIP_CSI_PIN);
    psramActive = true;
    return 0;
}

static struct lfs_config config = {
    // block device operations
    .read  = flash_read,
    .prog  = flash_prog,
    .erase = flash_erase,
    .sync  = flash_sync,

    // block device configuration
    .read_size = 1,
    .prog_size = 256,
    .block_size = 4096,
    .block_count = 256,
    .cache_size = 256,
    .lookahead_size = 16,
    .block_cycles = 500,
};

void fs_init(void) {
    if (lfs_mount(&mounts[2], &config) < 0) {
        lfs_format(&mounts[2], &config);
        lfs_mount(&mounts[2], &config);
    }
}
