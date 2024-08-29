#include "screen.hpp"
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <timers.h>
#include <string.h>
#include <hardware/gpio.h>
#include "drivers/st7789/st7789.hpp"
#include "drivers/rgbled/rgbled.hpp"
#include "common/pimoroni_bus.hpp"
#include "libraries/pico_graphics/pico_graphics.hpp"

static pimoroni::ST7789 display(320, 240, pimoroni::Rotation::ROTATE_0, false, pimoroni::get_spi_pins(pimoroni::BG_SPI_FRONT));
static pimoroni::RGBLED led(26, 27, 28);
uint16_t framebuffer[320 * 240];
uint32_t rgbLEDColor = 0x004000;
static pimoroni::PicoGraphics_PenRGB565 graphics(320, 240, framebuffer);
static SemaphoreHandle_t screen_sem;

static void screen_task(void*) {
    while (true) {
        xSemaphoreTake(screen_sem, portMAX_DELAY);
        display.update(&graphics);
        led.set_rgb((rgbLEDColor >> 16) & 0xFF, (rgbLEDColor >> 8) & 0xFF, (rgbLEDColor >> 0) & 0xFF);
    }
}

static void enable_graphics(TimerHandle_t timer) {
    display.set_backlight(255);
}

int screen_init(void) {
    display.set_backlight(0);
    led.set_rgb(0, 0, 0);
    memset(framebuffer, 0, sizeof(framebuffer));
    display.update(&graphics);
    screen_sem = xSemaphoreCreateBinary();
    xTaskCreate(screen_task, "screen", 512, NULL, 1, NULL);
    char id = 10;
    xTimerStart(xTimerCreate("graphics enable", pdMS_TO_TICKS(250), pdFALSE, &id, enable_graphics), 0);
    return 0;
}

void screen_update(void) {
    xSemaphoreGive(screen_sem);
}

void screen_deinit(void) {

}