/**
 * Memory-Mapped Filesystem for ESP32
 * A performance-oriented filesystem driver designed for read-only partitions.
 * 
 * Copyright (c) 2024 JackMacWindows. Licensed under the Apache 2.0 license.
 */

#ifndef MMFS_H
#define MMFS_H
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>

struct mmfs_mount;
typedef struct DIR DIR;
struct dirent {
    unsigned short int d_reclen;
    unsigned char d_type;
    char d_name[256];
};

#ifndef S_IFREG
#define S_IFREG 010000
#define S_IFDIR 020000
#endif
#define DT_REG 0
#define DT_DIR 1

extern struct mmfs_mount* mmfs_mount(const void* base_addr);
extern void mmfs_unmount(struct mmfs_mount* mount);

extern off_t mmfs_lseek(struct mmfs_mount* mount, int fd, off_t size, int mode);
extern ssize_t mmfs_read(struct mmfs_mount* mount, int fd, void* dst, size_t size);
extern ssize_t mmfs_pread(struct mmfs_mount* mount, int fd, void* dst, size_t size, off_t offset);
extern ssize_t mmfs_getbuf(struct mmfs_mount* mount, int fd, const void** buf);
extern int mmfs_open(struct mmfs_mount* mount, const char* path, int flags, int mode);
extern int mmfs_close(struct mmfs_mount* mount, int fd);
extern int mmfs_stat(struct mmfs_mount* mount, const char* path, struct stat* st);
extern DIR* mmfs_opendir(struct mmfs_mount* mount, const char* path);
extern struct dirent* mmfs_readdir(struct mmfs_mount* mount, DIR* pdir);
extern long mmfs_telldir(struct mmfs_mount* mount, DIR* pdir);
extern void mmfs_seekdir(struct mmfs_mount* mount, DIR* pdir, long offset);
extern int mmfs_closedir(struct mmfs_mount* mount, DIR* pdir);
extern int mmfs_access(struct mmfs_mount* mount, const char* path, int amode);

#endif
