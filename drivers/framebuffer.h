#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include "../cpu/types.h"

/* Linear framebuffer delivered by VBE (Bochs/QEMU). When enabled, this
 * replaces the text-mode screen as the primary display. */
#define FB_WIDTH   800
#define FB_HEIGHT  600
#define FB_BPP     32
#define FB_LFB     0xFD000000

/* Bochs VBE host registers, reached through the 0x1CE / 0x1CF ports. */
#define VBE_INDEX_PORT 0x01CE
#define VBE_DATA_PORT  0x01CF

#define VBE_DISPI_INDEX_ID      0x0
#define VBE_DISPI_INDEX_XRES    0x1
#define VBE_DISPI_INDEX_YRES    0x2
#define VBE_DISPI_INDEX_BPP     0x3
#define VBE_DISPI_INDEX_ENABLE  0x4

#define VBE_DISPI_DISABLED      0x00
#define VBE_DISPI_ENABLED       0x01
#define VBE_DISPI_LFB_ENABLED   0x40

/* Detect the adapter, switch to FB_WIDTH x FB_HEIGHT x FB_BPP and clear
 * the screen to black. Call once after the GDT is live. */
void fb_init(void);

/* Stores the active geometry into the given pointers (may be NULL). */
void fb_info(u32 *w, u32 *h, u32 *bpp);

/* 32bpp doubleword at (x,y): rgb = 0x00RRGGBB. */
void fb_putpixel(u32 x, u32 y, u32 rgb);
void fb_fillrect(u32 x, u32 y, u32 w, u32 h, u32 rgb);
void fb_clear(u32 rgb);

/* 8x16 bitmap font (ASCII 32..127), one glyph per cell. Pixels where a
 * glyph bit is set take 'fg'; unset pixels keep whatever is behind. */
#define FB_GLYPH_W 8
#define FB_GLYPH_H 16
#define FB_COLS (FB_WIDTH / FB_GLYPH_W)
#define FB_ROWS (FB_HEIGHT / FB_GLYPH_H)
void fb_draw_char(u32 x, u32 y, char c, u32 fg);
void fb_draw_string(u32 x, u32 y, const char *s, u32 fg);

/* Text console layered over the framebuffer: advancing cursor, '\n',
 * '\b', and scroll inside the active viewport. kprint() mirrors output
 * here so the shell is visible in graphics mode. The viewport defaults
 * to the area below the splash banner; gui.c rebinds it to the
 * terminal window. */
#define FB_CONSOLE_FG 0x00EAEAEA
#define FB_CONSOLE_BG 0x00182028
void fb_putc(char c);
void fb_print(const char *s);
void fb_backspace(void);
void fb_console_clear(void);

/* Confine the console to a pixel rectangle (cell-aligned) and home the
 * cursor; output outside is discarded via fb_console_disable(). */
void fb_console_set_viewport(u32 px, u32 py, u32 pw, u32 ph);
void fb_console_disable(void);

/* Rebind the viewport to a new pixel rectangle while KEEPING the
 * scrollback history and cursor position; repaints from the buffer.
 * Used when the terminal window is dragged around the desktop. */
void fb_console_move_viewport(u32 px, u32 py, u32 pw, u32 ph);

/* Scrollback: the console retains a line history; the viewport shows a
 * window into it. New output snaps the view back to the live bottom. */
void fb_console_scroll(int lines);   /* + = toward older, - = newer */
int fb_console_scroll_off(void);     /* lines the view is above bottom */
int fb_console_max_scroll(void);     /* max useful scroll_off */
int fb_console_history_lines(void);  /* lines currently retained */
int fb_console_view_rows(void);      /* visible text rows */

/* Static ZenOS boot splash: border, header bar and color bands. */
void fb_splash(void);

#endif