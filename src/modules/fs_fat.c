#include "fs.h"
#include <stdint.h>
#include <FreeRTOS.h>
#include <task.h>
#include <ff_headers.h>
#include <ff_sddisk.h>
#include <ff_stdio.h>
#include <ff_utils.h>
#include <ff_dir.h>
#include <pico/aon_timer.h>

typedef struct {
    struct mount * mount;
    FF_FILE *file;
} F_FILE;

#define FP(fp) (((F_FILE*)(fp))->file)

time_t FreeRTOS_time(time_t* pxTime) {
    struct timespec time;
    aon_timer_get_time(&time);
    if (pxTime) *pxTime = time.tv_sec;
    return time.tv_sec;
}

static FILE_base * fs_fat_fopen(struct mount * mount, const char * file, const char * mode, craftos_machine_t machine) {
    F_FILE * retval = (F_FILE*)malloc(sizeof(F_FILE));
    retval->file = ff_fopen(file, mode);
    if (retval->file == NULL) {
        free(retval);
        return NULL;
    }
    retval->mount = mount;
    return (FILE_base*)retval;
}

static int fs_fat_fclose(struct mount * mount, FILE_base *fp, craftos_machine_t machine) {
    int err = ff_fclose(FP(fp));
    if (err != 0) return err;
    free(fp);
    return 0;
}

static size_t fs_fat_fread(struct mount * mount, void * buf, size_t size, size_t count, FILE_base * fp, craftos_machine_t machine) {
    return ff_fread(buf, size, count, FP(fp));
}

static size_t fs_fat_fwrite(struct mount * mount, const void * buf, size_t size, size_t count, FILE_base * fp, craftos_machine_t machine) {
    return ff_fwrite(buf, size, count, FP(fp));
}

static int fs_fat_fflush(struct mount * mount, FILE_base * fp, craftos_machine_t machine) {
    return FF_FlushCache(mount->fat.disk->pxIOManager);
}

static int fs_fat_fgetc(struct mount * mount, FILE_base * fp, craftos_machine_t machine) {
    return ff_fgetc(FP(fp));
}

static int fs_fat_fputc(struct mount * mount, int ch, FILE_base * fp, craftos_machine_t machine) {
    return ff_fputc(ch, FP(fp));
}

static long fs_fat_ftell(struct mount * mount, FILE_base * fp, craftos_machine_t machine) {
    return ff_ftell(FP(fp));
}

static int fs_fat_fseek(struct mount * mount, FILE_base * fp, long offset, int origin, craftos_machine_t machine) {
    return ff_fseek(FP(fp), offset, origin);
}

static int fs_fat_feof(struct mount * mount, FILE_base * fp, craftos_machine_t machine) {
    return ff_feof(FP(fp));
}

static int fs_fat_ferror(struct mount * mount, FILE_base * fp, craftos_machine_t machine) {
    return ((F_FILE*)fp)->file->ulValidFlags;
}

static int fs_fat_remove(struct mount * mount, const char * path, craftos_machine_t machine) {
    return ff_remove(path);
}

static int fs_fat_rename(struct mount * mount, const char * from, const char * to, craftos_machine_t machine) {
    return ff_rename(from, to, 0);
}

static int fs_fat_mkdir(struct mount * mount, const char * path, int mode, craftos_machine_t machine) {
    return ff_mkdir(path);
}

static int fs_fat_access(struct mount * mount, const char * path, int flags, craftos_machine_t machine) {
    struct FF_STAT info;
    int err = ff_stat(path, &info);
    if (err == FF_ERR_FILE_NOT_FOUND) return 0;
    if (err != 0) return err;
    return info.st_mode & flags;
}

static int fs_fat_stat(struct mount * mount, const char * path, struct craftos_stat * st, craftos_machine_t machine) {
    if (path[2] == '\0' || (path[2] == '/' && path[3] == '\0')) {
        st->st_dev = path[0] - 'A';
        st->st_ino = 0;
        st->st_mode = 0040777;
        st->st_nlink = 0;
        st->st_uid = 0;
        st->st_gid = 0;
        st->st_rdev = 0;
        st->st_size = 0;
        st->st_blksize = 1;
        st->st_blocks = 0;
        st->st_atim.tv_sec = 0;
        st->st_ctim.tv_sec = 0;
        st->st_mtim.tv_sec = 0;
        st->st_atim.tv_nsec = 0;
        st->st_ctim.tv_nsec = 0;
        st->st_mtim.tv_nsec = 0;
        return 0;
    }
    struct FF_STAT info;
    int err = ff_stat(path, &info);
    if (err != 0) return err;
    st->st_dev = path[0] - 'A';
    st->st_ino = info.st_ino;
    st->st_mode = info.st_mode;
    st->st_nlink = 0;
    st->st_uid = 0;
    st->st_gid = 0;
    st->st_rdev = 0;
    st->st_size = info.st_size;
    st->st_blksize = 1;
    st->st_blocks = info.st_size;
    st->st_atim.tv_sec = info.ff_atime;
    st->st_ctim.tv_sec = info.ff_ctime;
    st->st_mtim.tv_sec = info.ff_mtime;
    st->st_atim.tv_nsec = 0;
    st->st_ctim.tv_nsec = 0;
    st->st_mtim.tv_nsec = 0;
    return 0;
}

static int fs_fat_statvfs(struct mount * mount, const char * path, struct craftos_statvfs * st, craftos_machine_t machine) {
    st->f_bsize = FF_GetPartitionBlockSize(mount->fat.disk->pxIOManager);
    st->f_frsize = 1;
    st->f_blocks = FF_GetVolumeSize(mount->fat.disk->pxIOManager) / st->f_bsize;
    st->f_bfree = FF_GetFreeSize(mount->fat.disk->pxIOManager, NULL) / st->f_bsize;
    st->f_bavail = st->f_bfree;
    st->f_files = 0;
    st->f_ffree = 0xFFFFFFFF;
    st->f_favail = 0xFFFFFFFF;
    st->f_fsid = path[0] - 'A';
    st->f_flag = 0;
    st->f_namemax = ffconfigMAX_FILENAME;
    return 0;
}

struct F_DIR {
    struct mount * mount;
    FF_DirEnt_t dir;
    struct craftos_dirent ent;
    int ok;
};

static DIR_base * fs_fat_opendir(struct mount * mount, const char * path, craftos_machine_t machine) {
    char _path[260];
    struct F_DIR * dir = (struct F_DIR*)malloc(sizeof(struct F_DIR));
    strcpy(_path, path + 2);
    strcat(_path, "/");
    int err = FF_FindFirst(mount->fat.disk->pxIOManager, &dir->dir, _path);
    if (err != 0) {
        free(dir);
        return NULL;
    }
    dir->mount = mount;
    dir->ok = 1;
    return (DIR_base*)dir;
}

static int fs_fat_closedir(struct mount * mount, DIR_base * dir, craftos_machine_t machine) {
    struct F_DIR * d = (struct F_DIR*)dir;
    /* TODO: is this needed? */
    while (FF_FindNext(d->mount->fat.disk->pxIOManager, &d->dir) == FF_ERR_NONE);
    free(dir);
    return 0;
}

static struct craftos_dirent * fs_fat_readdir(struct mount * mount, DIR_base * dir, craftos_machine_t machine) {
    struct F_DIR * d = (struct F_DIR*)dir;
    if (!d->ok) return NULL;
    strncpy(d->ent.d_name, d->dir.pcFileName, ffconfigMAX_FILENAME);
    d->ent.d_namlen = strlen(d->dir.pcFileName);
    d->ent.d_reclen = sizeof(craftos_dirent);
    d->ent.d_type = d->dir.ucAttrib;
    d->ok = FF_FindNext(d->mount->fat.disk->pxIOManager, &d->dir) == FF_ERR_NONE;
    return &d->ent;
}

const struct fs_impl fs_fat = {
    fs_fat_fopen,
    fs_fat_fclose,
    fs_fat_fread,
    fs_fat_fwrite,
    fs_fat_fflush,
    fs_fat_fgetc,
    fs_fat_fputc,
    fs_fat_ftell,
    fs_fat_fseek,
    fs_fat_feof,
    fs_fat_ferror,
    fs_fat_remove,
    fs_fat_rename,
    fs_fat_mkdir,
    fs_fat_access,
    fs_fat_stat,
    fs_fat_statvfs,
    fs_fat_opendir,
    fs_fat_closedir,
    fs_fat_readdir
};
