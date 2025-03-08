#ifndef __WAVOS__IO__SCREEN_H
#define __WAVOS__IO__SCREEN_H
#include <common/types.h>
/* text mode color consts */
enum vga_color {
    COLOR_BLACK = 0,
    COLOR_BLUE = 1,
    COLOR_GREEN = 2,
	COLOR_CYAN = 3,
	COLOR_RED = 4,
	COLOR_MAGENTA = 5,
	COLOR_BROWN = 6,
	COLOR_LIGHT_GREY = 7,
	COLOR_DARK_GREY = 8,
	COLOR_LIGHT_BLUE = 9,
	COLOR_LIGHT_GREEN = 10,
	COLOR_LIGHT_CYAN = 11,
	COLOR_LIGHT_RED = 12,
	COLOR_LIGHT_MAGENTA = 13,
	COLOR_LIGHT_BROWN = 14,
	COLOR_WHITE = 15,
};

void terminal_init(void);
void terminal_set_color(uint8_t);

void terminal_write(const char*, size_t);
void terminal_write_string(const char*);
void terminal_write_int(int, int);

void terminal_rem(void);

#endif