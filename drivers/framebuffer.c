#include "framebuffer.h"
#include "font8x16.h"
#include "../cpu/ports.h"
#include "../drivers/serial.h"
#include "../libc/mem.h"

/* Bochs VBE (also implemented by QEMU's std VGA): a 16-bit index/data
 * register pair at 0x1CE/0x1CF programs the video mode; the pixel data
 * then lives in the linear framebuffer at 0xFD000000. */

static void draw_glyph(u32 x, u32 y, u8 idx, u32 fg, u32 scale);
static void draw_string_scaled(u32 x, u32 y, const char *s, u32 fg,
                               u32 scale);

/* Rows 0..CON_TOP_ROW-1 stay reserved for the splash banner; scrolling
 * happens only inside [CON_TOP_ROW, FB_ROWS). Colors come from
 * framebuffer.h so gui.c can paint matching window interiors. */
#define CON_TOP_ROW 3
#define CON_FG FB_CONSOLE_FG
#define CON_BG FB_CONSOLE_BG

static int fb_ready = 0;

/* Console viewport: the cell rectangle text is confined to. Defaults to
 * the full area below the splash banner; gui.c rebinds it to the
 * terminal window's content area. */
static u32 view_x = 0;
static u32 view_y = CON_TOP_ROW * FB_GLYPH_H;
static u32 view_cols = FB_WIDTH / FB_GLYPH_W;
static u32 view_rows = FB_ROWS - CON_TOP_ROW;
static u32 view_pw = 0; /* full pixel width of the viewport rect */

/* ---- Scrollback text buffer ------------------------------------------
 * The console keeps its output as text cells in a ring of lines, so the
 * viewport can show older content. sb_head counts lines ever started
 * (the line being written is sb_head-1); scroll_off is how far the view
 * sits above the live bottom. */
#define SB_LINES 64
static char sbuf[SB_LINES][FB_COLS];
static u32 sb_head = 1;   /* next line number to allocate */
static u32 sb_col = 0;    /* write column inside the current line */
static int scroll_off = 0;

static u32 sb_base(void) {
    return sb_head > SB_LINES ? sb_head - SB_LINES : 0;
}

static int sb_max_scroll(void) {
    int retained = (int)(sb_head - sb_base());
    int m = retained - (int)view_rows;
    return m > 0 ? m : 0;
}

/* Clamp scroll_off into [0, max] for the current history/viewport. */
static void sb_clamp(void) {
    int m = sb_max_scroll();
    if (scroll_off > m) scroll_off = m;
    if (scroll_off < 0) scroll_off = 0;
}

/* Absolute index of the topmost visible line. */
static int sb_top(void) {
    int base = (int)sb_base();
    int top = (int)sb_head - (int)view_rows - scroll_off;
    if (top < base) top = base;
    if (top > (int)sb_head) top = (int)sb_head;
    return top;
}

static void draw_glyph(u32 x, u32 y, u8 idx, u32 fg, u32 scale);

/* Repaint every visible row from the text buffer. */
static void render_view(void) {
    if (!fb_ready || view_rows == 0) return;
    int top = sb_top();
    for (u32 r = 0; r < view_rows; r++) {
        int abs = top + (int)r;
        u32 py = view_y + r * FB_GLYPH_H;
        /* Fill the whole viewport width so no stale sliver survives
         * between the last glyph column and the rect's right edge. */
        fb_fillrect(view_x, py, view_pw ? view_pw
                                        : view_cols * FB_GLYPH_W,
                    FB_GLYPH_H, CON_BG);
        if (abs < 0 || abs >= (int)sb_head) continue;
        const char *ln = sbuf[abs % SB_LINES];
        for (u32 c = 0; c < view_cols; c++) {
            char ch = ln[c];
            if (ch >= 32 && ch <= 127)
                draw_glyph(view_x + c * FB_GLYPH_W, py, ch - 32, CON_FG,
                           1);
        }
    }
}

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
    fb_ready = 1;
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

    /* Title text, drawn last so nothing covers the glyphs. */
    fb_draw_string(12, 15, "ZenOS", 0x00FFFFFF);
    draw_string_scaled(FB_WIDTH / 2 - (5 * FB_GLYPH_W * 4) / 2, 168, "ZenOS",
                       0x00FFFFFF, 4);
    fb_draw_string(FB_WIDTH / 2 - 40, 246, "800x600x32", 0x00A8D8E8);

    /* The console starts on the first row below the header bar. */
    view_x = 0;
    view_y = CON_TOP_ROW * FB_GLYPH_H;
    view_cols = FB_WIDTH / FB_GLYPH_W;
    view_rows = FB_ROWS - CON_TOP_ROW;
    sb_head = 1;
    sb_col = 0;
    scroll_off = 0;
    for (u32 l = 0; l < SB_LINES; l++)
        for (u32 i = 0; i < FB_COLS; i++) sbuf[l][i] = 0;
}

/* ---- Bitmap font rendering ------------------------------------------ */

/* Blits glyph 'idx' (font8x16 entry) with each font pixel expanded to
 * scale x scale. Only set bits are written, so glyphs composite onto
 * whatever background is already on screen. Caller keeps the rect on
 * screen. */
static void draw_glyph(u32 x, u32 y, u8 idx, u32 fg, u32 scale) {
    const unsigned char *g = &font8x16[idx * FB_GLYPH_H];
    for (u32 r = 0; r < FB_GLYPH_H; r++) {
        unsigned char bits = g[r];
        if (!bits) continue;
        for (u32 c = 0; c < FB_GLYPH_W; c++) {
            if (!((bits >> (7 - c)) & 1)) continue;
            fb_fillrect(x + c * scale, y + r * scale, scale, scale, fg);
        }
    }
}

static void draw_string_scaled(u32 x, u32 y, const char *s, u32 fg,
                               u32 scale) {
    for (u32 i = 0; s[i]; i++) {
        unsigned char ch = s[i];
        if (ch < 32 || ch > 127) ch = '?';
        draw_glyph(x + i * FB_GLYPH_W * scale, y, ch - 32, fg, scale);
    }
}

void fb_draw_char(u32 x, u32 y, char c, u32 fg) {
    if (x + FB_GLYPH_W > FB_WIDTH || y + FB_GLYPH_H > FB_HEIGHT) return;
    unsigned char ch = c;
    if (ch < 32 || ch > 127) ch = '?';
    draw_glyph(x, y, ch - 32, fg, 1);
}

void fb_draw_string(u32 x, u32 y, const char *s, u32 fg) {
    draw_string_scaled(x, y, s, fg, 1);
}

/* ---- Text console ---------------------------------------------------- */

void fb_console_set_viewport(u32 px, u32 py, u32 pw, u32 ph) {
    view_x = px;
    view_y = py;
    view_cols = pw / FB_GLYPH_W;
    view_rows = ph / FB_GLYPH_H;
    view_pw = pw;
    sb_head = 1;
    sb_col = 0;
    scroll_off = 0;
    for (u32 l = 0; l < SB_LINES; l++)
        for (u32 i = 0; i < FB_COLS; i++) sbuf[l][i] = 0;
}

void fb_console_disable(void) { view_rows = 0; }

void fb_console_move_viewport(u32 px, u32 py, u32 pw, u32 ph) {
    view_x = px;
    view_y = py;
    view_cols = pw / FB_GLYPH_W;
    view_rows = ph / FB_GLYPH_H;
    view_pw = pw;
    sb_clamp();
    render_view();
}

/* Shift the visible pixel rows up one text row inside the viewport
 * (column-exact, so neighbouring windows are untouched), then clear the
 * freed bottom row. Used while the view is live. */
static void shift_pixels(void) {
    if (!fb_ready || view_rows == 0) return;
    u32 row_bytes = view_cols * FB_GLYPH_W * 4;
    u32 xoff = view_x * 4;
    for (u32 gr = 0; gr < (view_rows - 1) * FB_GLYPH_H; gr++) {
        u8 *dst =
            (u8 *)(FB_LFB + (view_y + gr) * FB_WIDTH * 4 + xoff);
        u8 *src = dst + FB_WIDTH * 4 * FB_GLYPH_H;
        memory_copy(src, dst, row_bytes);
    }
    fb_fillrect(view_x, view_y + (view_rows - 1) * FB_GLYPH_H,
                view_cols * FB_GLYPH_W, FB_GLYPH_H, CON_BG);
}

/* Start a fresh line: bump head, clear its ring slot. While the user is
 * scrolled up, grow scroll_off so the view stays anchored to the same
 * content instead of following the output. */
static void newline(void) {
    int top = sb_top();
    int row = (int)(sb_head - 1) - top; /* row just completed */

    sb_head++;
    sb_col = 0;
    char *ln = sbuf[(sb_head - 1) % SB_LINES];
    for (u32 i = 0; i < FB_COLS; i++) ln[i] = 0;
    if (scroll_off > 0) {
        scroll_off++;
        sb_clamp();
        return;
    }
    if (row >= 0 && row < (int)view_rows - 1) {
        /* Empty rows remain below: just advance to the next row (and
         * clear it) instead of scrolling the top line out of view. */
        fb_fillrect(view_x, view_y + (u32)(row + 1) * FB_GLYPH_H,
                    view_cols * FB_GLYPH_W, FB_GLYPH_H, CON_BG);
        return;
    }
    shift_pixels();
}

void fb_putc(char c) {
    if (!fb_ready || view_rows == 0) return;
    if (scroll_off > 0) { /* new output snaps the view to the bottom */
        scroll_off = 0;
        render_view();
    }
    char *ln = sbuf[(sb_head - 1) % SB_LINES];
    int top = sb_top();
    int row = (int)(sb_head - 1) - top;
    if (c == '\n') {
        newline();
    } else if (c == '\b') {
        if (sb_col > 0) {
            sb_col--;
            ln[sb_col] = 0;
            if (row >= 0 && row < (int)view_rows)
                fb_fillrect(view_x + sb_col * FB_GLYPH_W,
                            view_y + row * FB_GLYPH_H, FB_GLYPH_W,
                            FB_GLYPH_H, CON_BG);
        }
    } else {
        unsigned char ch = c;
        if (ch >= 32 && ch <= 127) {
            ln[sb_col] = ch;
            if (row >= 0 && row < (int)view_rows)
                draw_glyph(view_x + sb_col * FB_GLYPH_W,
                           view_y + row * FB_GLYPH_H, ch - 32, CON_FG,
                           1);
        }
        sb_col++;
    }
    if (sb_col >= view_cols) newline();
}

void fb_print(const char *s) {
    for (u32 i = 0; s[i]; i++) fb_putc(s[i]);
}

void fb_backspace(void) { fb_putc('\b'); }

void fb_console_clear(void) {
    if (!fb_ready) return;
    fb_fillrect(view_x, view_y, view_cols * FB_GLYPH_W,
                view_rows * FB_GLYPH_H, CON_BG);
    sb_head = 1;
    sb_col = 0;
    scroll_off = 0;
    for (u32 l = 0; l < SB_LINES; l++)
        for (u32 i = 0; i < FB_COLS; i++) sbuf[l][i] = 0;
}

void fb_console_scroll(int lines) {
    if (view_rows == 0) return;
    scroll_off += lines;
    sb_clamp();
    render_view();
}

int fb_console_scroll_off(void) { return scroll_off; }

int fb_console_max_scroll(void) { return sb_max_scroll(); }

int fb_console_history_lines(void) {
    return (int)(sb_head - sb_base());
}

int fb_console_view_rows(void) { return (int)view_rows; }