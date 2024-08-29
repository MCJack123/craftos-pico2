/**
 * Memory-Mapped Filesystem for ESP32
 * A performance-oriented filesystem driver designed for read-only partitions.
 * 
 * Copyright (c) 2024 JackMacWindows. Licensed under the Apache 2.0 license.
 */

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include "mmfs.h"

#define MMFS_MAGIC 0x73664D4D // MMfs
#define MAX_FDS 128
#define PATH_MAX 260

struct mmfs_dir_ent {
    const char name[24];
    const unsigned is_dir: 1;
    const unsigned size: 31;
    const uint32_t offset;
} __packed;

struct mmfs_dir {
    const uint32_t magic;
    const uint32_t count;
    const struct mmfs_dir_ent entries[];
} __packed;

struct mmfs_fd {
    const uint8_t* start;
    const uint8_t* ptr;
    const uint8_t* end;
};

struct mmfs_dir_iter {
    uint16_t dd_vfs_idx; /*!< VFS index, not to be used by applications */
    uint16_t dd_rsv;     /*!< field reserved for future extension */
    uint8_t offset;
    const struct mmfs_dir* dir;
    struct dirent ent;
};

struct mmfs_mount {
    union {
        const struct mmfs_dir* root;
        const void* start;
    };
    struct mmfs_fd fds[MAX_FDS];
    struct mmfs_dir_iter dirs[MAX_FDS/8];
    struct mmfs_mount* next;
};

static const struct mmfs_dir_ent* mmfs_traverse(struct mmfs_mount* mount, const char* pat) {
    // Directory entries are sorted, so we use a binary sort on each level
    static char path[PATH_MAX];
    strcpy(path, pat);
    const struct mmfs_dir_ent* node = NULL;
    const struct mmfs_dir* dir = mount->root;
    for (char* p = strtok(path, "/"); p; p = strtok(NULL, "/")) {
        if (strcmp(p, "") == 0) continue;
        if (node) {
            if (!node->is_dir) {
                errno = ENOTDIR;
                return NULL;
            }
            dir = mount->start + node->offset;
        }
        if (dir->magic != MMFS_MAGIC) {
            errno = EIO;
            return NULL;
        }
        uint32_t l = 0, h = dir->count;
        while (true) {
            if (l >= h) {
                errno = ENOENT;
                return NULL;
            }
            uint32_t m = l + (h - l) / 2;
            int res = strcmp(p, dir->entries[m].name);
            if (res == 0) {
                node = &dir->entries[m];
                break;
            } else if (res > 0) {
                l = m + 1;
            } else {
                h = m;
            }
        }
    }
    return node;
}

off_t mmfs_lseek(struct mmfs_mount* mount, int fd, off_t size, int mode) {
    if (fd < 0 || fd >= MAX_FDS) {
        errno = EBADF;
        return -1;
    }
    struct mmfs_fd* fh = &mount->fds[fd];
    if (!fh->start) {
        errno = EBADF;
        return -1;
    }
    switch (mode) {
        case SEEK_SET:
            fh->ptr = fh->start + size;
            break;
        case SEEK_CUR:
            fh->ptr += size;
            break;
        case SEEK_END:
            fh->ptr = fh->end - size;
            break;
        default:
            errno = EINVAL;
            return -1;
    }
    return fh->ptr - fh->start;
}

ssize_t mmfs_read(struct mmfs_mount* mount, int fd, void* dst, size_t size) {
    if (fd < 0 || fd >= MAX_FDS) {
        errno = EBADF;
        return -1;
    }
    struct mmfs_fd* fh = &mount->fds[fd];
    if (!fh->start) {
        errno = EBADF;
        return -1;
    }
    if (fh->ptr >= fh->end) return 0;
    if (fh->ptr + size >= fh->end) size = fh->end - fh->ptr;
    if (size) memcpy(dst, fh->ptr, size);
    fh->ptr += size;
    return size;
}

ssize_t mmfs_pread(struct mmfs_mount* mount, int fd, void* dst, size_t size, off_t offset) {
    if (fd < 0 || fd >= MAX_FDS) {
        errno = EBADF;
        return -1;
    }
    struct mmfs_fd* fh = &mount->fds[fd];
    if (!fh->start) {
        errno = EBADF;
        return -1;
    }
    if (offset < 0 || fh->start + offset >= fh->end) return 0;
    if (fh->ptr + offset + size >= fh->end) size = fh->end - (fh->start + offset);
    if (size) memcpy(dst, fh->start + offset, size);
    return size;
}

ssize_t mmfs_getbuf(struct mmfs_mount* mount, int fd, const void** buf) {
    if (fd < 0 || fd >= MAX_FDS) {
        errno = EBADF;
        return -1;
    }
    struct mmfs_fd* fh = &mount->fds[fd];
    if (!fh->start) {
        errno = EBADF;
        return -1;
    }
    *buf = fh->start;
    return fh->end - fh->start;
}

int mmfs_open(struct mmfs_mount* mount, const char* path, int flags, int mode) {
    if ((flags & O_ACCMODE) != O_RDONLY) {
        errno = EACCES;
        return -1;
    }
    for (int i = 0; i < MAX_FDS; i++) {
        if (mount->fds[i].start == NULL) {
            const struct mmfs_dir_ent* ent = mmfs_traverse(mount, path);
            if (ent == NULL) return -1;
            if (ent->is_dir) {
                errno = EISDIR;
                return -1;
            }
            mount->fds[i].start = mount->fds[i].ptr = mount->start + ent->offset;
            mount->fds[i].end = mount->start + ent->offset + ent->size;
            return i;
        }
    }
    errno = ENFILE;
    return -1;
}

int mmfs_close(struct mmfs_mount* mount, int fd) {
    if (fd < 0 || fd >= MAX_FDS) {
        errno = EBADF;
        return -1;
    }
    struct mmfs_fd* fh = &mount->fds[fd];
    if (!fh->start) {
        errno = EBADF;
        return -1;
    }
    fh->start = fh->ptr = fh->end = NULL;
    return 0;
}

int mmfs_stat(struct mmfs_mount* mount, const char* path, struct stat* st) {
    st->st_atime = st->st_ctime = st->st_mtime = 0;
    st->st_gid = st->st_uid = 0;
    st->st_blksize = 1;
    st->st_dev = 0;
    st->st_ino = 0;
    st->st_nlink = 0;
    st->st_rdev = 0;
    if (strcmp(path, "/") == 0) {
        st->st_blocks = 0;
        st->st_mode = S_IFDIR | 0555;
        st->st_size = 0;
    } else {
        const struct mmfs_dir_ent* ent = mmfs_traverse(mount, path);
        if (ent == NULL) return -1;
        st->st_blocks = ent->size;
        st->st_mode = (ent->is_dir ? S_IFDIR : S_IFREG) | 0555;
        st->st_size = ent->size;
    }
    return 0;
}

DIR* mmfs_opendir(struct mmfs_mount* mount, const char* path) {
    for (int i = 0; i < MAX_FDS/8; i++) {
        if (mount->dirs[i].dir == NULL) {
            if (strcmp(path, "/") == 0 || strcmp(path, "") == 0) {
                mount->dirs[i].dir = mount->root;
            } else {
                const struct mmfs_dir_ent* ent = mmfs_traverse(mount, path);
                if (ent == NULL) return NULL;
                if (!ent->is_dir) {
                    errno = ENOTDIR;
                    return NULL;
                }
                mount->dirs[i].dir = mount->start + ent->offset;
            }
            mount->dirs[i].offset = 0;
            return (DIR*)&mount->dirs[i];
        }
    }
    errno = ENFILE;
    return NULL;
}

struct dirent* mmfs_readdir(struct mmfs_mount* mount, DIR* pdir) {
    (void)mount;
    struct mmfs_dir_iter* ent = (struct mmfs_dir_iter*)pdir;
    if (ent->dir->magic != MMFS_MAGIC) {
        errno = EIO;
        return NULL;
    }
    if (ent->offset >= ent->dir->count) return NULL;
    strcpy(ent->ent.d_name, ent->dir->entries[ent->offset].name);
    ent->ent.d_type = ent->dir->entries[ent->offset].is_dir ? DT_DIR : DT_REG;
    ent->offset++;
    return &ent->ent;
}

long mmfs_telldir(struct mmfs_mount* mount, DIR* pdir) {
    (void)mount;
    struct mmfs_dir_iter* ent = (struct mmfs_dir_iter*)pdir;
    if (ent->dir->magic != MMFS_MAGIC) {
        errno = EIO;
        return -1;
    }
    return ent->offset;
}

void mmfs_seekdir(struct mmfs_mount* mount, DIR* pdir, long offset) {
    (void)mount;
    struct mmfs_dir_iter* ent = (struct mmfs_dir_iter*)pdir;
    if (ent->dir->magic != MMFS_MAGIC) {
        errno = EIO;
        return;
    }
    ent->offset = offset;
}

int mmfs_closedir(struct mmfs_mount* mount, DIR* pdir) {
    (void)mount;
    struct mmfs_dir_iter* ent = (struct mmfs_dir_iter*)pdir;
    if (ent->dir->magic != MMFS_MAGIC) {
        errno = EIO;
        return -1;
    }
    ent->dir = NULL;
    return 0;
}

int mmfs_access(struct mmfs_mount* mount, const char* path, int amode) {
    const struct mmfs_dir_ent* ent = strcmp(path, "/") == 0 ? (struct mmfs_dir_ent*)mount : mmfs_traverse(mount, path);
    if (!ent) return -1;
    if (amode & W_OK) {
        errno = EACCES;
        return -1;
    }
    return 0;
}

struct mmfs_mount* mmfs_mount(const void* ptr) {
    if (*(uint32_t*)ptr != MMFS_MAGIC) {
        return NULL;
    }
    struct mmfs_mount* mount = malloc(sizeof(struct mmfs_mount));
    if (mount == NULL) {
        return NULL;
    }
    mount->root = ptr;
    for (int i = 0; i < MAX_FDS; i++)
        mount->fds[i].start = mount->fds[i].ptr = mount->fds[i].end = NULL;
    for (int i = 0; i < MAX_FDS/8; i++)
        mount->dirs[i].dir = NULL;
    return mount;
}

void mmfs_unmount(struct mmfs_mount* mount) {
    free(mount);
}
