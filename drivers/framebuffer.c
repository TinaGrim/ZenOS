#include "framebuffer.h"
#include "../cpu/ports.h"
#include "../drivers/serial.h"

/* Bochs VBE (also implemented by QEMU's std VGA): a 16-bit index/data
 * register pair at 0x1CE/0x1CF programs the video mode; the pixel data
 * then lives in the linear framebuffer at 0xFD000000. */
static u16 vbe_read(u16 idx) {
    port_word_out(VBE_INDEX_PORT, idx);
    return port_word_in(VBE_DATA_PORT);
}

static void vbe_write(u16 idx, u16 val) {
    port_word_out(VBE_INDEX_PORT, idx);
    port_word_out(VBE_DATA_PORT, val);
}

void fb_init(void) {
    u16 id = vbe_read(VBE_DISPI_INDEX_ID);
    if (id < 0xB0C0) {
        serial_write_str("fb:unsupported ");
        serial_write_int((int)id);
        serial_write_str("\n");
        return;
    }

    vbe_write(VBE_DISPI_INDEX_XRES, FB_WIDTH);
    vbe_write(VBE_DISPI_INDEX_YRES, FB_HEIGHT);
    vbe_write(VBE_DISPI_INDEX_BPP, FB_BPP);
    vbe_write(VBE_DISPI_INDEX_ENABLE,
              VBE_DISPI_ENABLED | VBE_DISPI_LFB_ENABLED);

    if (!(vbe_read(VBE_DISPI_INDEX_ENABLE) & VBE_DISPI_ENABLED)) {
        serial_write_str("fb:mode-failed\n");
        return;
    }

    fb_clear(0x00000000);
    serial_write_str("fb:init-ok\n");
}

void fb_info(u32 *w, u32 *h, u32 *bpp) {
    if (w) *w = FB_WIDTH;
    if (h) *h = FB_HEIGHT;
    if (bpp) *bpp = FB_BPP;
}

void fb_putpixel(u32 x, u32 y, u32 rgb) {
    if (x >= FB_WIDTH || y >= FB_HEIGHT) return;
    volatile u32 *p = (volatile u32 *)(FB_LFB + (y * FB_WIDTH + x) * 4);
    *p = rgb;
}

void fb_fillrect(u32 x, u32 y, u32 w, u32 h, u32 rgb) {
    if (x >= FB_WIDTH || y >= FB_HEIGHT) return;
    if (x + w > FB_WIDTH) w = FB_WIDTH - x;
    if (y + h > FB_HEIGHT) h = FB_HEIGHT - y;

    volatile u32 *base = (volatile u32 *)(FB_LFB + (y * FB_WIDTH + x) * 4);
    for (u32 r = 0; r < h; r++) {
        volatile u32 *p = base + r * FB_WIDTH;
        for (u32 c = 0; c < w; c++) p[c] = rgb;
    }
}

void fb_clear(u32 rgb) {
    volatile u32 *p = (volatile u32 *)FB_LFB;
    for (u32 i = 0; i < FB_WIDTH * FB_HEIGHT; i++) p[i] = rgb;
}

void fb_splash(void) {
    fb_clear(0x00182028);

    /* Header bar (drawn before the border so the border stays on top). */
    fb_fillrect(0, 0, FB_WIDTH, 46, 0x002088A8);

    /* ZenOS "logo": teal panel with a lighter inset. */
    fb_fillrect(FB_WIDTH / 2 - 180, 110, 360, 180, 0x000E3A50);
    fb_fillrect(FB_WIDTH / 2 - 160, 130, 320, 140, 0x001A6E8C);

    /* Three gradient bars animating the palette. */
    fb_fillrect(FB_WIDTH / 4, 360, FB_WIDTH / 2, 28, 0x00E8512A);
    fb_fillrect(FB_WIDTH / 4, 392, FB_WIDTH / 2, 28, 0x00F0A520);
    fb_fillrect(FB_WIDTH / 4, 424, FB_WIDTH / 2, 28, 0x0038C06A);

    /* Outer border (on top of everything). */
    fb_fillrect(0, 0, FB_WIDTH, 2, 0x00FFFFFF);
    fb_fillrect(0, FB_HEIGHT - 2, FB_WIDTH, 2, 0x00FFFFFF);
    fb_fillrect(0, 0, 2, FB_HEIGHT, 0x00FFFFFF);
    fb_fillrect(FB_WIDTH - 2, 0, 2, FB_HEIGHT, 0x00FFFFFF);
}