#include <string.h>
#include <FreeRTOS.h>
#include <timers.h>
#include <semphr.h>
#include <hardware/gpio.h>
#include "terminal.hpp"
#include "font.h"
#include "../drivers/screen.hpp"

#define SCREEN_WIDTH (FB_UWIDTH/6)
#define SCREEN_HEIGHT (FB_UHEIGHT/9)
#define SCREEN_SIZE (SCREEN_WIDTH*SCREEN_HEIGHT)

static char blinkTimerID = 'b';
static TimerHandle_t blinkTimer;
static TaskHandle_t timer;
static uint8_t screen[SCREEN_SIZE];
static uint8_t colors[SCREEN_SIZE];
static int cursorX = 0, cursorY = 0;
static bool cursorOn = false;
static int8_t cursorColor = -1;
static bool changed = true;

#define COLOR16(r, g, b, a) __builtin_bswap16(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3))

const uint16_t defaultPalette[16] = {
    COLOR16(0xf0, 0xf0, 0xf0, 0xFF),
    COLOR16(0xf2, 0xb2, 0x33, 0xFF),
    COLOR16(0xe5, 0x7f, 0xd8, 0xFF),
    COLOR16(0x99, 0xb2, 0xf2, 0xFF),
    COLOR16(0xde, 0xde, 0x6c, 0xFF),
    COLOR16(0x7f, 0xcc, 0x19, 0xFF),
    COLOR16(0xf2, 0xb2, 0xcc, 0xFF),
    COLOR16(0x4c, 0x4c, 0x4c, 0xFF),
    COLOR16(0x99, 0x99, 0x99, 0xFF),
    COLOR16(0x4c, 0x99, 0xb2, 0xFF),
    COLOR16(0xb2, 0x66, 0xe5, 0xFF),
    COLOR16(0x33, 0x66, 0xcc, 0xFF),
    COLOR16(0x7f, 0x66, 0x4c, 0xFF),
    COLOR16(0x57, 0xa6, 0x4e, 0xFF),
    COLOR16(0xcc, 0x4c, 0x4c, 0xFF),
    COLOR16(0x11, 0x11, 0x11, 0xFF)
};

uint16_t palette[16];

static void terminal_task(void*) {
    while (true) {
        if (changed) {
            changed = false;
            TickType_t start = xTaskGetTickCount();
            for (int y = 0; y < SCREEN_HEIGHT * 9; y++) {
                uint16_t* line = &framebuffer[y*FB_WIDTH];
                for (int x = 0; x < SCREEN_WIDTH * 6; x++) {
                    const int cp = (y / 9) * SCREEN_WIDTH + (x / 6);
                    const uint8_t c = screen[cp];
                    line[x] = font_data[((c >> 4) * 9 + (y % 9)) * 96 + ((c & 0xF) * 6 + (x % 6))] ? palette[colors[cp] & 0x0F] : palette[colors[cp] >> 4];
                }
                int cx = cursorX;
                if (cursorOn && y == cursorY * 9 + 7 && cx >= 0 && cx < TERM_WIDTH) memset(line + cx * 6, palette[cursorColor], 12);
            }
            //rgbLEDColor = cursorOn ? 0x004000 : 0x004000;
            screen_update();
        }
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}

static void terminal_blink_task(TimerHandle_t timer) {
    if (cursorColor >= 0) {cursorOn = !cursorOn; changed = true;}
}

void terminal_init(void) {
    memcpy(palette, defaultPalette, 32);
    xTaskCreate(terminal_task, "terminal", 1536, NULL, 4, &timer);
    blinkTimer = xTimerCreate("terminalBlink", pdMS_TO_TICKS(400), pdTRUE, &blinkTimerID, terminal_blink_task);
    xTimerStart(blinkTimer, 0);
    terminal_clear(-1, 0xF0);
}

void terminal_deinit(void) {
    xTimerStop(blinkTimer, portMAX_DELAY);
    xTimerDelete(blinkTimer, portMAX_DELAY);
    vTaskDelete(timer);
}

void terminal_clear(int line, uint8_t col) {
    if (line < 0) {
        memset(screen, ' ', SCREEN_SIZE);
        memset(colors, col, SCREEN_SIZE);
    } else {
        memset(screen + line*SCREEN_WIDTH, ' ', SCREEN_WIDTH);
        memset(colors + line*SCREEN_WIDTH, col, SCREEN_WIDTH);
    }
    changed = true;
}

void terminal_scroll(int lines, uint8_t col) {
    if (lines > 0 ? (unsigned)lines >= SCREEN_HEIGHT : (unsigned)-lines >= SCREEN_HEIGHT) {
        // scrolling more than the height is equivalent to clearing the screen
        memset(screen, ' ', SCREEN_HEIGHT * SCREEN_WIDTH);
        memset(colors, col, SCREEN_HEIGHT * SCREEN_WIDTH);
    } else if (lines > 0) {
        memmove(screen, screen + lines * SCREEN_WIDTH, (SCREEN_HEIGHT - lines) * SCREEN_WIDTH);
        memset(screen + (SCREEN_HEIGHT - lines) * SCREEN_WIDTH, ' ', lines * SCREEN_WIDTH);
        memmove(colors, colors + lines * SCREEN_WIDTH, (SCREEN_HEIGHT - lines) * SCREEN_WIDTH);
        memset(colors + (SCREEN_HEIGHT - lines) * SCREEN_WIDTH, col, lines * SCREEN_WIDTH);
    } else if (lines < 0) {
        memmove(screen - lines * SCREEN_WIDTH, screen, (SCREEN_HEIGHT + lines) * SCREEN_WIDTH);
        memset(screen, ' ', -lines * SCREEN_WIDTH);
        memmove(colors - lines * SCREEN_WIDTH, colors, (SCREEN_HEIGHT + lines) * SCREEN_WIDTH);
        memset(colors, col, -lines * SCREEN_WIDTH);
    }
    changed = true;
}

void terminal_write(int x, int y, const uint8_t* text, int len, uint8_t col) {
    if (y < 0 || y >= SCREEN_HEIGHT || x >= SCREEN_WIDTH) return;
    if (x < 0) {
        text += x;
        len -= x;
        x = 0;
    }
    if (len <= 0) return;
    if (x + len > SCREEN_WIDTH) len = SCREEN_WIDTH - x;
    if (len <= 0) return;
    memcpy(screen + y*SCREEN_WIDTH + x, text, len);
    memset(colors + y*SCREEN_WIDTH + x, col, len);
    changed = true;
}

void terminal_blit(int x, int y, const uint8_t* text, const uint8_t* col, int len) {
    if (y < 0 || y >= SCREEN_HEIGHT || x >= SCREEN_WIDTH) return;
    if (x < 0) {
        text += x;
        col += x;
        len -= x;
        x = 0;
    }
    if (len <= 0) return;
    if (x + len > SCREEN_WIDTH) len = SCREEN_WIDTH - x;
    if (len <= 0) return;
    memcpy(screen + y*SCREEN_WIDTH + x, text, len);
    memcpy(colors + y*SCREEN_WIDTH + x, col, len);
    changed = true;
}

void terminal_cursor(int8_t color, int x, int y) {
    cursorColor = color;
    cursorX = x;
    cursorY = y;
    if (cursorColor < 0) cursorOn = false;
    changed = true;
}
