#include "fs.h"
#include <stdint.h>

typedef struct {
    struct mount * mount;
    lfs_file_t file;
} F_FILE;

#define FP(fp) &((F_FILE*)(fp))->mount->lfs, &((F_FILE*)(fp))->file

static FILE_base * fs_lfs_fopen(struct mount * mount, const char * file, const char * mode, craftos_machine_t machine) {
    F_FILE * retval = (F_FILE*)malloc(sizeof(F_FILE));
    int err = lfs_file_open(&mount->lfs, &retval->file, file + 2, (mode[0] == 'r' ? LFS_O_RDONLY : LFS_O_WRONLY | LFS_O_CREAT | (mode[0] == 'a' ? LFS_O_APPEND : LFS_O_TRUNC)));
    if (err != 0) {
        free(retval);
        return NULL;
    }
    retval->mount = mount;
    return (FILE_base*)retval;
}

static int fs_lfs_fclose(struct mount * mount, FILE_base *fp, craftos_machine_t machine) {
    int err = lfs_file_close(FP(fp));
    if (err != 0) return err;
    free(fp);
    return 0;
}

static size_t fs_lfs_fread(struct mount * mount, void * buf, size_t size, size_t count, FILE_base * fp, craftos_machine_t machine) {
    return lfs_file_read(FP(fp), buf, size * count);
}

static size_t fs_lfs_fwrite(struct mount * mount, const void * buf, size_t size, size_t count, FILE_base * fp, craftos_machine_t machine) {
    return lfs_file_write(FP(fp), buf, size * count);
}

static int fs_lfs_fflush(struct mount * mount, FILE_base * fp, craftos_machine_t machine) {
    return lfs_file_sync(FP(fp));
}

static int fs_lfs_fgetc(struct mount * mount, FILE_base * fp, craftos_machine_t machine) {
    uint8_t c;
    int sz = lfs_file_read(FP(fp), &c, 1);
    if (sz == 0) return EOF;
    else if (sz < 0) return sz;
    else return c;
}

static int fs_lfs_fputc(struct mount * mount, int ch, FILE_base * fp, craftos_machine_t machine) {
    uint8_t c = ch;
    int err = lfs_file_write(FP(fp), &ch, 1);
    if (err > 0) return 0;
    else return err;
}

static long fs_lfs_ftell(struct mount * mount, FILE_base * fp, craftos_machine_t machine) {
    return lfs_file_tell(FP(fp));
}

static int fs_lfs_fseek(struct mount * mount, FILE_base * fp, long offset, int origin, craftos_machine_t machine) {
    return lfs_file_seek(FP(fp), offset, origin);
}

static int fs_lfs_feof(struct mount * mount, FILE_base * fp, craftos_machine_t machine) {
    return lfs_file_tell(FP(fp)) >= lfs_file_size(FP(fp));
}

static int fs_lfs_ferror(struct mount * mount, FILE_base * fp, craftos_machine_t machine) {
    return ((F_FILE*)fp)->file.flags & LFS_F_ERRED;
}

static int fs_lfs_remove(struct mount * mount, const char * path, craftos_machine_t machine) {
    return lfs_remove(&mount->lfs, path + 2);
}

static int fs_lfs_rename(struct mount * mount, const char * from, const char * to, craftos_machine_t machine) {
    return lfs_rename(&mount->lfs, from + 2, to + 2);
}

static int fs_lfs_mkdir(struct mount * mount, const char * path, int mode, craftos_machine_t machine) {
    return lfs_mkdir(&mount->lfs, path + 2);
}

static int fs_lfs_access(struct mount * mount, const char * path, int flags, craftos_machine_t machine) {
    struct lfs_info info;
    int err = lfs_stat(&mount->lfs, path + 2, &info);
    if (err == LFS_ERR_NOENT) return 0;
    if (err != 0) return err;
    return 0;
}

static int fs_lfs_stat(struct mount * mount, const char * path, struct craftos_stat * st, craftos_machine_t machine) {
    if (path[3] == '\0' || (path[3] == '/' && path[4] == '\0')) {
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
    struct lfs_info info;
    int err = lfs_stat(&mount->lfs, path + 2, &info);
    if (err != 0) return err;
    st->st_dev = path[0] - 'A';
    st->st_ino = 0;
    st->st_mode = 0777 | (info.type == LFS_TYPE_REG ? 0100000 : 0040000);
    st->st_nlink = 0;
    st->st_uid = 0;
    st->st_gid = 0;
    st->st_rdev = 0;
    st->st_size = info.size;
    st->st_blksize = 1;
    st->st_blocks = info.size;
    st->st_atim.tv_sec = 0;
    st->st_ctim.tv_sec = 0;
    st->st_mtim.tv_sec = 0;
    st->st_atim.tv_nsec = 0;
    st->st_ctim.tv_nsec = 0;
    st->st_mtim.tv_nsec = 0;
    return 0;
}

static int fs_lfs_statvfs(struct mount * mount, const char * path, struct craftos_statvfs * st, craftos_machine_t machine) {
    struct lfs_fsinfo info;
    int err = lfs_fs_stat(&mount->lfs, &info);
    if (err != 0) return err;
    ssize_t size = lfs_fs_size(&mount->lfs);
    if (size < 0) return size;
    st->f_bsize = info.block_size;
    st->f_frsize = 1;
    st->f_blocks = info.block_count;
    st->f_bfree = info.block_count - size;
    st->f_bavail = info.block_count - size;
    st->f_files = 0;
    st->f_ffree = 0xFFFFFFFF;
    st->f_favail = 0xFFFFFFFF;
    st->f_fsid = path[0] - 'A';
    st->f_flag = 0;
    st->f_namemax = info.name_max;
    return 0;
}

struct F_DIR {
    struct mount * mount;
    lfs_dir_t dir;
    craftos_dirent ent;
};

static DIR_base * fs_lfs_opendir(struct mount * mount, const char * path, craftos_machine_t machine) {
    struct F_DIR * dir = (struct F_DIR*)malloc(sizeof(struct F_DIR));
    int err = lfs_dir_open(&mount->lfs, &dir->dir, path + 2);
    if (err != 0) {
        free(dir);
        return NULL;
    }
    dir->mount = mount;
    return (DIR_base*)dir;
}

static int fs_lfs_closedir(struct mount * mount, DIR_base * dir, craftos_machine_t machine) {
    int err = lfs_dir_close(&((struct F_DIR*)dir)->mount->lfs, &((struct F_DIR*)dir)->dir);
    if (err != 0) return err;
    free(dir);
    return 0;
}

static struct craftos_dirent * fs_lfs_readdir(struct mount * mount, DIR_base * dir, craftos_machine_t machine) {
    struct F_DIR * d = (struct F_DIR*)dir;
    struct lfs_info info;
    int err = lfs_dir_read(&d->mount->lfs, &d->dir, &info);
    if (err == 0) return NULL;
    strncpy(d->ent.d_name, info.name, 256);
    d->ent.d_namlen = strnlen(info.name, LFS_NAME_MAX+1);
    d->ent.d_reclen = sizeof(craftos_dirent);
    d->ent.d_type = info.type;
    return &d->ent;
}

const struct fs_impl fs_lfs = {
    fs_lfs_fopen,
    fs_lfs_fclose,
    fs_lfs_fread,
    fs_lfs_fwrite,
    fs_lfs_fflush,
    fs_lfs_fgetc,
    fs_lfs_fputc,
    fs_lfs_ftell,
    fs_lfs_fseek,
    fs_lfs_feof,
    fs_lfs_ferror,
    fs_lfs_remove,
    fs_lfs_rename,
    fs_lfs_mkdir,
    fs_lfs_access,
    fs_lfs_stat,
    fs_lfs_statvfs,
    fs_lfs_opendir,
    fs_lfs_closedir,
    fs_lfs_readdir
};
