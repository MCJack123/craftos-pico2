#include <FreeRTOS.h>
#include <task.h>
#include <pico/platform.h>
#include <hardware/exception.h>
#include <sfe_pico_alloc.h>
#include "drivers/screen.hpp"
#include "drivers/hid.hpp"
#include "modules/terminal.hpp"

extern "C" void fs_init(void);
extern void machine_main(void*);

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
    screen_init();
    terminal_init();
    fs_init();
    hid_init();

    terminal_clear(-1, 0xF0);
    //terminal_write_literal(0, 0, "Starting CraftOS-Pico2...", 0xF4);
    terminal_cursor(-1, 0, 0);
    machine_main(arg);
}

int main(void) {
    exception_set_exclusive_handler(HARDFAULT_EXCEPTION, hardfault_handler);
    xTaskCreate(app_main, "main", 16384, NULL, 3, &mainTask);
    vTaskStartScheduler();
    return 0;
}