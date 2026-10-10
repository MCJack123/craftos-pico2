#include <craftos.h>
#include <stdint.h>
#include <stdlib.h>
#include <pico/time.h>
#include <pico/aon_timer.h>
#include <FreeRTOS.h>
#include <timers.h>
#include <semphr.h>
#include "event.hpp"
#include "modules/fs.h"

typedef struct timer {
    int id;
    TimerHandle_t timer;
    struct timer* next;
} timer_ll_t;

static int nextTimerID = 0;
static timer_ll_t* timer_ll_head = NULL, *timer_ll_tail = NULL;

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
    return xSemaphoreTake((SemaphoreHandle_t)mutex, portMAX_DELAY) ? 0 : -1;
}

static void F_mutex_unlock(craftos_mutex_t mutex) {
    xSemaphoreGive((SemaphoreHandle_t)mutex);
}

// paths are [A-F]:/dir/file
static FILE * F_fopen(const char * file, const char * mode, craftos_machine_t machine) {
    if (file[0] < 'A' || file[0] > 'F') return NULL;
    struct mount * m = &mounts[file[0] - 'A'];
    if (m->type == FS_MOUNT_TYPE_NONE || m->impl == NULL) return NULL;
    return (FILE*)m->impl->fopen(m, file, mode, machine);
}

static int F_fclose(FILE *fp, craftos_machine_t machine) {
    struct mount * m = ((FILE_base*)fp)->mount;
    if (m->type == FS_MOUNT_TYPE_NONE || m->impl == NULL) return -1;
    return m->impl->fclose(m, (FILE_base*)fp, machine);
}

static size_t F_fread(void * buf, size_t size, size_t count, FILE * fp, craftos_machine_t machine) {
    struct mount * m = ((FILE_base*)fp)->mount;
    if (m->type == FS_MOUNT_TYPE_NONE || m->impl == NULL) return 0;
    return m->impl->fread(m, buf, size, count, (FILE_base*)fp, machine);
}

static size_t F_fwrite(const void * buf, size_t size, size_t count, FILE * fp, craftos_machine_t machine) {
    struct mount * m = ((FILE_base*)fp)->mount;
    if (m->type == FS_MOUNT_TYPE_NONE || m->impl == NULL) return 0;
    return m->impl->fwrite(m, buf, size, count, (FILE_base*)fp, machine);
}

static int F_fflush(FILE * fp, craftos_machine_t machine) {
    struct mount * m = ((FILE_base*)fp)->mount;
    if (m->type == FS_MOUNT_TYPE_NONE || m->impl == NULL) return -1;
    return m->impl->fflush(m, (FILE_base*)fp, machine);
}

static int F_fgetc(FILE * fp, craftos_machine_t machine) {
    struct mount * m = ((FILE_base*)fp)->mount;
    if (m->type == FS_MOUNT_TYPE_NONE || m->impl == NULL) return -1;
    return m->impl->fgetc(m, (FILE_base*)fp, machine);
}

static int F_fputc(int ch, FILE * fp, craftos_machine_t machine) {
    struct mount * m = ((FILE_base*)fp)->mount;
    if (m->type == FS_MOUNT_TYPE_NONE || m->impl == NULL) return -1;
    return m->impl->fputc(m, ch, (FILE_base*)fp, machine);
}

static long F_ftell(FILE * fp, craftos_machine_t machine) {
    struct mount * m = ((FILE_base*)fp)->mount;
    if (m->type == FS_MOUNT_TYPE_NONE || m->impl == NULL) return -1;
    return m->impl->ftell(m, (FILE_base*)fp, machine);
}

static int F_fseek(FILE * fp, long offset, int origin, craftos_machine_t machine) {
    struct mount * m = ((FILE_base*)fp)->mount;
    if (m->type == FS_MOUNT_TYPE_NONE || m->impl == NULL) return -1;
    return m->impl->fseek(m, (FILE_base*)fp, offset, origin, machine);
}

static int F_feof(FILE * fp, craftos_machine_t machine) {
    struct mount * m = ((FILE_base*)fp)->mount;
    if (m->type == FS_MOUNT_TYPE_NONE || m->impl == NULL) return -1;
    return m->impl->feof(m, (FILE_base*)fp, machine);
}

static int F_ferror(FILE * fp, craftos_machine_t machine) {
    struct mount * m = ((FILE_base*)fp)->mount;
    if (m->type == FS_MOUNT_TYPE_NONE || m->impl == NULL) return -1;
    return m->impl->ferror(m, (FILE_base*)fp, machine);
}

static int F_remove(const char * path, craftos_machine_t machine) {
    if (path[0] < 'A' || path[0] > 'F') return -1;
    struct mount * m = &mounts[path[0] - 'A'];
    if (m->type == FS_MOUNT_TYPE_NONE || m->impl == NULL) return -1;
    return m->impl->remove(m, path, machine);
}

static int F_rename(const char * from, const char * to, craftos_machine_t machine) {
    if (from[0] < 'A' || from[0] > 'F' || from[0] != to[0]) return -1;
    struct mount * m = &mounts[from[0] - 'A'];
    if (m->type == FS_MOUNT_TYPE_NONE || m->impl == NULL) return -1;
    return m->impl->rename(m, from, to, machine);
}

static int F_mkdir(const char * path, int mode, craftos_machine_t machine) {
    if (path[0] < 'A' || path[0] > 'F') return -1;
    struct mount * m = &mounts[path[0] - 'A'];
    if (m->type == FS_MOUNT_TYPE_NONE || m->impl == NULL) return -1;
    return m->impl->mkdir(m, path, mode, machine);
}

static int F_access(const char * path, int flags, craftos_machine_t machine) {
    if (path[0] < 'A' || path[0] > 'F') return -1;
    struct mount * m = &mounts[path[0] - 'A'];
    if (m->type == FS_MOUNT_TYPE_NONE || m->impl == NULL) return -1;
    return m->impl->access(m, path, flags, machine);
}

static int F_stat(const char * path, struct craftos_stat * st, craftos_machine_t machine) {
    if (path[0] < 'A' || path[0] > 'F') return -1;
    struct mount * m = &mounts[path[0] - 'A'];
    if (m->type == FS_MOUNT_TYPE_NONE || m->impl == NULL) return -1;
    return m->impl->stat(m, path, st, machine);
}

static int F_statvfs(const char * path, struct craftos_statvfs * st, craftos_machine_t machine) {
    if (path[0] < 'A' || path[0] > 'F') return -1;
    struct mount * m = &mounts[path[0] - 'A'];
    if (m->type == FS_MOUNT_TYPE_NONE || m->impl == NULL) return -1;
    return m->impl->statvfs(m, path, st, machine);
}

static craftos_DIR * F_opendir(const char * path, craftos_machine_t machine) {
    if (path[0] < 'A' || path[0] > 'F') return NULL;
    struct mount * m = &mounts[path[0] - 'A'];
    if (m->type == FS_MOUNT_TYPE_NONE || m->impl == NULL) return NULL;
    return (craftos_DIR*)m->impl->opendir(m, path, machine);
}

static int F_closedir(craftos_DIR * dir, craftos_machine_t machine) {
    struct mount * m = ((DIR_base*)dir)->mount;
    if (m->type == FS_MOUNT_TYPE_NONE || m->impl == NULL) return -1;
    return m->impl->closedir(m, (DIR_base*)dir, machine);
}

static struct craftos_dirent * F_readdir(craftos_DIR * dir, craftos_machine_t machine) {
    struct mount * m = ((DIR_base*)dir)->mount;
    if (m->type == FS_MOUNT_TYPE_NONE || m->impl == NULL) return NULL;
    return m->impl->readdir(m, (DIR_base*)dir, machine);
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
