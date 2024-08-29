#ifndef SCREEN_H
#define SCREEN_H
#include "../common.hpp"

#define FB_WIDTH    320
#define FB_HEIGHT   240
#define FB_UWIDTH   320
#define FB_UHEIGHT  240

extern uint16_t framebuffer[FB_UWIDTH * FB_UHEIGHT];
extern uint32_t rgbLEDColor;

extern int screen_init(void);
extern void screen_update(void);
extern void screen_deinit(void);

#endif
