#include <FreeRTOS.h>
#include <task.h>
#include <pico/platform.h>
#include <pico/aon_timer.h>
#include <hardware/exception.h>
#include <sfe_pico_alloc.h>
#include <craftos.h>
#include "drivers/screen.hpp"
#include "drivers/hid.hpp"
#include "modules/terminal.hpp"

extern "C" void fs_init(void);
extern void machine_main(void*);
extern "C" const craftos_func_t F_func;
extern void peripheral_detect_init(void);

TaskHandle_t mainTask;

extern "C" {
    void vApplicationStackOverflowHook( TaskHandle_t xTask,
                                        char * pcTaskName )
    {
        /* Check pcTaskName for the name of the offending task,
         * or pxCurrentTCB if pcTaskName has itself been corrupted. */
        panic("stack overflow for task %s", pcTaskName);
    }

    void __not_in_flash_func(hardfault_handler)(void) {
        panic("hard fault");
    }
}

void app_main(void* arg) {
    craftos_init(&F_func);
    screen_init();
    terminal_init();
    fs_init();
    hid_init();
    peripheral_detect_init();
    timespec now;
    now.tv_nsec = 0;
    now.tv_sec = 1790812800; // 2026-10-01T00:00:00Z
    aon_timer_start(&now);
    machine_main(arg);
}

int main(void) {
    exception_set_exclusive_handler(HARDFAULT_EXCEPTION, hardfault_handler);
    xTaskCreate(app_main, "main", 16384, NULL, 3, &mainTask);
    vTaskStartScheduler();
    return 0;
}