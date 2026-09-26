#ifndef TERMINAL_H
#define TERMINAL_H
#include <stdint.h>

#define TERM_WIDTH 53
#define TERM_HEIGHT 26
#define NO_CURSOR -1

extern const uint16_t defaultPalette[16];
extern uint16_t palette[16];

extern void terminal_init(void);
extern void terminal_deinit(void);

#endif