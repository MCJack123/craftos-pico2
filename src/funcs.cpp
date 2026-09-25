#include <craftos.h>
#include <stdint.h>
#include <stdlib.h>
#include <pico/time.h>
#include <pico/aon_timer.h>
#include <FreeRTOS.h>
#include <timers.h>
#include <semphr.h>
#include <lfs.h>
#include "event.hpp"

typedef struct timer {
    int id;
    TimerHandle_t timer;
    struct timer* next;
} timer_ll_t;

static int nextTimerID = 0;
static timer_ll_t* timer_ll_head = NULL, *timer_ll_tail = NULL;

extern "C" {
    lfs_t mounts[3];
}

static double F_timestamp() {
    struct timespec ts;
    if (!aon_timer_get_time(&ts)) return to_ms_since_boot(get_absolute_time());
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

static unsigned long F_convertPixelValue(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    return __builtin_bswap16(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3));
}

static void timer(TimerHandle_t timer) {
    int id = (int)(ptrdiff_t)pvTimerGetTimerID(timer);
    event_t event;
    event.type = EVENT_TYPE_TIMER;
    event.timer.timerID = id;
    if (event_push_isr(&event) == pdTRUE) portYIELD_FROM_ISR(1);
    timer_ll_t* tm = timer_ll_head, *last = NULL;
    while (tm) {
        if (tm->id == id) {
            if (last) last->next = tm->next;
            if (timer_ll_head == tm) timer_ll_head = tm->next;
            if (timer_ll_tail == tm) timer_ll_tail = last;
            free(tm);
            xTimerDelete(timer, portMAX_DELAY);
            return;
        }
        last = tm;
        tm = tm->next;
    }
}

static int F_startTimer(unsigned long millis, craftos_machine_t machine) {
    int id = nextTimerID++;
    int ticks = pdMS_TO_TICKS(millis);
    if (ticks <= 0) {
        event_t event;
        event.type = EVENT_TYPE_TIMER;
        event.timer.timerID = id;
        event_push(&event);
    } else {
        TimerHandle_t handle = xTimerCreate("timer", ticks, pdFALSE, (void*)(ptrdiff_t)id, timer);
        xTimerStart(handle, portMAX_DELAY);
        timer_ll_t* tm = (timer_ll_t*)malloc(sizeof(timer_ll_t));
        tm->id = id;
        tm->timer = handle;
        tm->next = NULL;
        if (timer_ll_tail) timer_ll_tail->next = tm;
        else timer_ll_head = timer_ll_tail = tm;
    }
    return id;
}

static void F_cancelTimer(int id, craftos_machine_t machine) {
    timer_ll_t* tm = timer_ll_head, *last = NULL;
    while (tm) {
        if (tm->id == id) {
            xTimerStop(tm->timer, portMAX_DELAY);
            xTimerDelete(tm->timer, portMAX_DELAY);
            if (last) last->next = tm->next;
            if (timer_ll_head == tm) timer_ll_head = tm->next;
            if (timer_ll_tail == tm) timer_ll_tail = last;
            free(tm);
            return;
        }
        last = tm;
        tm = tm->next;
    }
}

static void F_setComputerLabel(const char * label, craftos_machine_t machine) {

}

static uint16_t F_redstone_getInput(craftos_redstone_side_t side, craftos_machine_t machine) {

    return 0;
}

static void F_redstone_setOutput(craftos_redstone_side_t side, unsigned short value, craftos_machine_t machine) {

}

static craftos_mutex_t F_mutex_create() {
    return xSemaphoreCreateMutex();
}

static void F_mutex_destroy(craftos_mutex_t mutex) {
    vSemaphoreDelete(mutex);
}

static int F_mutex_lock(craftos_mutex_t mutex) {
    return xSemaphoreTake((QueueHandle_t)mutex, portMAX_DELAY) ? 0 : -1;
}

static void F_mutex_unlock(craftos_mutex_t mutex) {
    xSemaphoreGive((SemaphoreHandle_t)mutex);
}

typedef struct {
    lfs_file_t file;
    lfs_t * mount;
} F_FILE;

#define FP(fp) ((F_FILE*)(fp))->mount, &((F_FILE*)(fp))->file

// paths are [A-C]:/dir/file
static FILE * F_fopen(const char * file, const char * mode, craftos_machine_t machine) {
    F_FILE * retval = (F_FILE*)malloc(sizeof(F_FILE));
    int err = lfs_file_open(&mounts[file[0] - 'A'], &retval->file, file + 3, (mode[0] == 'r' ? LFS_O_RDONLY : LFS_O_WRONLY | LFS_O_CREAT | (mode[0] == 'a' ? LFS_O_APPEND : LFS_O_TRUNC)));
    if (err != 0) {
        free(retval);
        return NULL;
    }
    retval->mount = &mounts[file[0] - 'A'];
    return (FILE*)retval;
}

static int F_fclose(FILE *fp, craftos_machine_t machine) {
    int err = lfs_file_close(FP(fp));
    if (err != 0) return err;
    free(fp);
    return 0;
}

static size_t F_fread(void * buf, size_t size, size_t count, FILE * fp, craftos_machine_t machine) {
    return lfs_file_read(FP(fp), buf, size * count);
}

static size_t F_fwrite(const void * buf, size_t size, size_t count, FILE * fp, craftos_machine_t machine) {
    return lfs_file_write(FP(fp), buf, size * count);
}

static int F_fflush(FILE * fp, craftos_machine_t machine) {
    return lfs_file_sync(FP(fp));
}

static int F_fgetc(FILE * fp, craftos_machine_t machine) {
    uint8_t c;
    int sz = lfs_file_read(FP(fp), &c, 1);
    if (sz == 0) return EOF;
    else if (sz < 0) return sz;
    else return c;
}

static int F_fputc(int ch, FILE * fp, craftos_machine_t machine) {
    uint8_t c = ch;
    int err = lfs_file_write(FP(fp), &ch, 1);
    if (err > 0) return 0;
    else return err;
}

static long F_ftell(FILE * fp, craftos_machine_t machine) {
    return lfs_file_tell(FP(fp));
}

static int F_fseek(FILE * fp, long offset, int origin, craftos_machine_t machine) {
    return lfs_file_seek(FP(fp), offset, origin);
}

static int F_feof(FILE * fp, craftos_machine_t machine) {
    return lfs_file_tell(FP(fp)) >= lfs_file_size(FP(fp));
}

static int F_ferror(FILE * fp, craftos_machine_t machine) {
    return ((F_FILE*)fp)->file.flags & LFS_F_ERRED;
}

// hack - these are never executed
extern "C" {
    void _unlink() {}
    void _link() {}
    void _stat() {}
    void mkdir() {}
}

static int F_remove(const char * path, craftos_machine_t machine) {
    return lfs_remove(&mounts[path[0] - 'A'], path + 3);
}

static int F_rename(const char * from, const char * to, craftos_machine_t machine) {
    return lfs_rename(&mounts[from[0] - 'A'], from + 3, to + 3);
}

static int F_mkdir(const char * path, int mode, craftos_machine_t machine) {
    return lfs_mkdir(&mounts[path[0] - 'A'], path + 3);
}

static int F_access(const char * path, int flags, craftos_machine_t machine) {
    lfs_info info;
    int err = lfs_stat(&mounts[path[0] - 'A'], path + 3, &info);
    if (err != 0) return err;
    if (info.type == LFS_TYPE_DIR) return -1;
    return 0;
}

static int F_stat(const char * path, struct craftos_stat * st, craftos_machine_t machine) {
    lfs_info info;
    int err = lfs_stat(&mounts[path[0] - 'A'], path + 3, &info);
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

static int F_statvfs(const char * path, struct craftos_statvfs * st, craftos_machine_t machine) {
    lfs_fsinfo info;
    int err = lfs_fs_stat(&mounts[path[0] - 'A'], &info);
    if (err != 0) return err;
    ssize_t size = lfs_fs_size(&mounts[path[0] - 'A']);
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
    lfs_dir_t dir;
    lfs_t * mount;
    craftos_dirent ent;
};

static craftos_DIR * F_opendir(const char * path, craftos_machine_t machine) {
    F_DIR * dir = (F_DIR*)malloc(sizeof(F_DIR));
    int err = lfs_dir_open(&mounts[path[0] - 'A'], &dir->dir, path + 3);
    if (err != 0) {
        free(dir);
        return NULL;
    }
    dir->mount = &mounts[path[0] - 'A'];
    return (craftos_DIR*)dir;
}

static int F_closedir(craftos_DIR * dir, craftos_machine_t machine) {
    int err = lfs_dir_close(((F_DIR*)dir)->mount, &((F_DIR*)dir)->dir);
    if (err != 0) return err;
    free(dir);
    return 0;
}

static struct craftos_dirent * F_readdir(craftos_DIR * dir, craftos_machine_t machine) {
    F_DIR * d = (F_DIR*)dir;
    lfs_info info;
    int err = lfs_dir_read(d->mount, &d->dir, &info);
    if (err == 0) return NULL;
    strncpy(d->ent.d_name, info.name, 256);
    d->ent.d_namlen = strnlen(info.name, LFS_NAME_MAX+1);
    d->ent.d_reclen = sizeof(craftos_dirent);
    d->ent.d_type = info.type;
    return &d->ent;
}

extern "C" const craftos_func_t F_func = {
    F_timestamp,
    F_convertPixelValue,
    F_startTimer,
    F_cancelTimer,
    F_setComputerLabel,
    F_redstone_getInput,
    F_redstone_setOutput,
    NULL,
    NULL,
    NULL,
    F_mutex_create,
    F_mutex_destroy,
    F_mutex_lock,
    F_mutex_unlock,
    F_fopen,
    F_fclose,
    F_fread,
    F_fwrite,
    F_fflush,
    F_fgetc,
    F_fputc,
    F_ftell,
    F_fseek,
    F_feof,
    F_ferror,
    F_remove,
    F_rename,
    F_mkdir,
    F_access,
    F_stat,
    F_statvfs,
    F_opendir,
    F_closedir,
    F_readdir,
    NULL /* TODO: HTTP */
};
