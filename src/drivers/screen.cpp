#include "screen.hpp"
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <timers.h>
#include <string.h>
#include <hardware/gpio.h>
#include <hardware/spi.h>
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
extern SemaphoreHandle_t * spi0_mutex;

static void screen_task(void*) {
    while (true) {
        xSemaphoreTake(screen_sem, portMAX_DELAY);
        SemaphoreHandle_t mutex = *spi0_mutex;
        if (mutex != NULL) xSemaphoreTake(mutex, portMAX_DELAY);
        // SD card sets baud rate (much) lower, reset to intended rate when drawing
        uint32_t baud = spi_get_baudrate(spi0);
        spi_set_baudrate(spi0, 62'500'000);
        display.update(&graphics);
        spi_set_baudrate(spi0, baud);
        if (mutex != NULL) xSemaphoreGive(mutex);
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
    display.set_backlight(0);
    led.set_rgb(0, 0, 0);
    memset(framebuffer, 0, sizeof(framebuffer));
}