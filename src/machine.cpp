#include <string.h>
extern "C" {
#include <sfe_psram.h>
}
#include <FreeRTOS.h>
#include <task.h>
#include "event.hpp"
#include <craftos.h>
#include <fcntl.h>
#include <hardware/flash.h>
#include <sfe_pico_alloc.h>

extern "C" {
    extern const unsigned char craftos2_rom[];
    craftos_machine_t machine = NULL;
}

static const char* eventNames[] = {
    "",
    "key",
    "key_up",
    "char",
    "timer",
    "alarm",
    "disk",
    "disk_eject",
    "paste",
    "redstone",
    "speaker_audio_empty",
    "terminate",
};

static bool forceLocalAlloc = false;

static void *l_alloc (void *ud, void *ptr, size_t osize, size_t nsize) {
  (void)ud; (void)osize;  /* not used */
  if (nsize == 0) {
    if (((ptrdiff_t)ptr & 0xFF000000) == 0x11000000)
        sfe_mem_free(ptr);
    else
        free(ptr);
    return NULL;
  }
  else if ((ptr == NULL && forceLocalAlloc) || (ptr != NULL && ((ptrdiff_t)ptr & 0xFF000000) != 0x11000000))
    return realloc(ptr, nsize);
  else
    return sfe_mem_realloc(ptr, nsize);
}

void machine_main(void*) {
    if (!sfe_pico_alloc_init()) forceLocalAlloc = true;

    craftos_machine_config_t config = {
        0,
        NULL, /* TODO: label */
        craftos2_rom,
        NULL,
        320,
        240,
        1,
        "C:/",
        "bios.use_multishell=false,shell.autocomplete=false",
        l_alloc,
        NULL
    };
    machine = craftos_machine_create(&config);
    craftos_status_t status;
    do {
        status = craftos_machine_run(machine);
        if (status == CRAFTOS_MACHINE_STATUS_YIELD) {
            event_t ev;
            event_wait(&ev);
            switch (ev.type) {
                case EVENT_TYPE_KEY: craftos_event_key(machine, ev.key.keycode, ev.key.repeat); break;
                case EVENT_TYPE_KEY_UP: craftos_event_key_up(machine, ev.key.keycode); break;
                case EVENT_TYPE_CHAR: craftos_event_char(machine, ev.character.c); break;
                case EVENT_TYPE_TIMER: craftos_event_timer(machine, ev.timer.timerID);
                case EVENT_TYPE_DISK: craftos_machine_queue_event(machine, "disk", "z", "left"); break; /* TODO: sides */
                case EVENT_TYPE_DISK_EJECT: craftos_machine_queue_event(machine, "disk_eject", "z", "left"); break; /* TODO: sides */
                case EVENT_TYPE_SPEAKER_AUDIO_EMPTY: craftos_machine_queue_event(machine, "speaker_audio_empty", "z", "left"); break; /* TODO: sides */
            }
        } else if (status == CRAFTOS_MACHINE_STATUS_RESTART) {

        } else if (status == CRAFTOS_MACHINE_STATUS_SHUTDOWN) {

        } else if (status == CRAFTOS_MACHINE_STATUS_ERROR) {
            //craftos_machine_destroy(machine);
            //machine = NULL;
            while (true) vTaskDelay(pdMS_TO_TICKS(1000));
            return;
        }
    } while (status == CRAFTOS_MACHINE_STATUS_YIELD);
    printf("Closing session.\n");
    //craftos_terminal_clear(machine->term, -1, 0xFE);
    craftos_terminal_write_literal(machine->term, 0, 0, "Error running computer", 0xFE);
    craftos_terminal_write_literal(machine->term, 0, 1, "ComputerCraft may be installed incorrectly", 0xFE);
    //craftos_machine_destroy(machine);
    //machine = NULL;
    while (true) vTaskDelay(pdMS_TO_TICKS(1000));
}
