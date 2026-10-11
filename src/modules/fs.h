#ifndef MODULES_FS_H
#define MODULES_FS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <craftos.h>
#include <lfs.h>
#include <ff_headers.h>

typedef enum {
    FS_MOUNT_TYPE_NONE,
    FS_MOUNT_TYPE_LITTLEFS,
    FS_MOUNT_TYPE_FAT_SD,
    FS_MOUNT_TYPE_FAT_USB,
    FS_MOUNT_TYPE_MMFS
} mount_type_t;

typedef struct {
    struct mount * mount;
    /* anything else can go after */
} FILE_base;
typedef FILE_base DIR_base; /* same base fields */

struct fs_impl {
    FILE_base * (*fopen)(struct mount * mount, const char * file, const char * mode, craftos_machine_t machine);
    int (*fclose)(struct mount * mount, FILE_base *fp, craftos_machine_t machine);
    size_t (*fread)(struct mount * mount, void * buf, size_t size, size_t count, FILE_base * fp, craftos_machine_t machine);
    size_t (*fwrite)(struct mount * mount, const void * buf, size_t size, size_t count, FILE_base * fp, craftos_machine_t machine);
    int (*fflush)(struct mount * mount, FILE_base * fp, craftos_machine_t machine);
    int (*fgetc)(struct mount * mount, FILE_base * fp, craftos_machine_t machine);
    int (*fputc)(struct mount * mount, int ch, FILE_base * fp, craftos_machine_t machine);
    long (*ftell)(struct mount * mount, FILE_base * fp, craftos_machine_t machine);
    int (*fseek)(struct mount * mount, FILE_base * fp, long offset, int origin, craftos_machine_t machine);
    int (*feof)(struct mount * mount, FILE_base * fp, craftos_machine_t machine);
    int (*ferror)(struct mount * mount, FILE_base * fp, craftos_machine_t machine);
    int (*remove)(struct mount * mount, const char * path, craftos_machine_t machine);
    int (*rename)(struct mount * mount, const char * from, const char * to, craftos_machine_t machine);
    int (*mkdir)(struct mount * mount, const char * path, int mode, craftos_machine_t machine);
    int (*access)(struct mount * mount, const char * path, int flags, craftos_machine_t machine);
    int (*stat)(struct mount * mount, const char * path, struct craftos_stat * st, craftos_machine_t machine);
    int (*statvfs)(struct mount * mount, const char * path, struct craftos_statvfs * st, craftos_machine_t machine);
    DIR_base * (*opendir)(struct mount * mount, const char * path, craftos_machine_t machine);
    int (*closedir)(struct mount * mount, DIR_base * dir, craftos_machine_t machine);
    struct craftos_dirent * (*readdir)(struct mount * mount, DIR_base * dir, craftos_machine_t machine);
};

struct mount {
    mount_type_t type;
    const struct fs_impl * impl;
    union {
        lfs_t lfs;
        struct {
            FF_Disk_t *disk;
        } fat;
        void * mmfs;
    };
};

extern struct mount mounts[6];
extern const struct fs_impl fs_lfs;
extern const struct fs_impl fs_fat;

void fs_init(void);

#ifdef __cplusplus
}
#endif

#endif