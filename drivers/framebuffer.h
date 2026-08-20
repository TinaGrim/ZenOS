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

/* Static ZenOS boot splash: border, header bar and color bands. */
void fb_splash(void);

#endif