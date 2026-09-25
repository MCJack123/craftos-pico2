#include <string.h>
#include <FreeRTOS.h>
#include <timers.h>
#include <semphr.h>
#include <hardware/gpio.h>
#include <craftos.h>
#include "terminal.hpp"
#include "../drivers/screen.hpp"

#define SCREEN_WIDTH (FB_UWIDTH/6)
#define SCREEN_HEIGHT (FB_UHEIGHT/9)
#define SCREEN_SIZE (SCREEN_WIDTH*SCREEN_HEIGHT)

static TaskHandle_t timer;

extern "C" craftos_machine_t machine;

static void terminal_task(void*) {
    while (machine == NULL) vTaskDelay(pdMS_TO_TICKS(30));
    TickType_t start = xTaskGetTickCount();
    while (true) {
        if (machine != NULL) craftos_terminal_render(machine->term, framebuffer, FB_WIDTH * 2, 16, 1, 1);
        //rgbLEDColor = cursorOn ? 0x004000 : 0x004000;
        screen_update();
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    vTaskDelete(NULL);
}

void terminal_init(void) {
    xTaskCreate(terminal_task, "terminal", 1536, NULL, 4, &timer);
    //blinkTimer = xTimerCreate("terminalBlink", pdMS_TO_TICKS(400), pdTRUE, &blinkTimerID, terminal_blink_task);
    //xTimerStart(blinkTimer, 0);
}

void terminal_deinit(void) {
    //xTimerStop(blinkTimer, portMAX_DELAY);
    //xTimerDelete(blinkTimer, portMAX_DELAY);
    vTaskDelete(timer);
}
