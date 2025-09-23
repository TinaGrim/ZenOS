#ifndef SCREEN_H
#define SCREEN_H

#include "../cpu/types.h"

#define VIDEO_ADDRESS 0xb8000
#define MAX_ROWS 25
#define MAX_COLS 80
#define WHITE_ON_BLACK 0x0f
#define RED_ON_WHITE 0xf4

/* Screen i/o ports */
#define REG_SCREEN_CTRL 0x3d4
#define REG_SCREEN_DATA 0x3d5

/* Public kernel API */
void clear_screen();
void kprint_at(const char *message, int col, int row);
void kprint(const char *message);
void kprint_backspace();

/* Cursor control */
void screen_move_cursor(int dx, int dy);
int screen_get_cursor_offset(void);
void screen_set_cursor_offset(int offset);

#endif