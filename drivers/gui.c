#include "gui.h"
#include "framebuffer.h"
#include "mouse.h"
#include "vmmouse.h"
#include "power.h"
#include "screen.h"
#include "../libc/files.h"
#include "../libc/function.h"
#include "../libc/string.h"
#include "../drivers/serial.h"

/* ---- Layout ---------------------------------------------------------- */

#define DESK_BG   0x00182028
#define BAR_BG    0x00102A38
#define BAR_LINE  0x002088A8
#define BTN_BG    0x001A4256
#define BTN_HOVER 0x00297A96
#define BTN_FG    0x00EAEAEA
#define WIN_BG    0x000E2A3A
#define WIN_TITLE 0x002088A8
#define WIN_LINE  0x00386B84
#define ICON_TERM 0x001A6E8C
#define ICON_FILE 0x00C4701C

#define TB_H 28
#define TB_Y (FB_HEIGHT - TB_H)

typedef struct {
    u16 x, y, w, h;
    const char *label;
} btn_t;

enum { B_TERMINAL, B_FILES, B_REBOOT, B_SHUTDOWN, BTN_COUNT };

static const btn_t btns[BTN_COUNT] = {
    {8, TB_Y + 3, 92, 22, "TERMINAL"},
    {104, TB_Y + 3, 76, 22, "FILES"},
    {596, TB_Y + 3, 88, 22, "REBOOT"},
    {688, TB_Y + 3, 104, 22, "SHUTDOWN"},
};

/* Desktop icons: single click runs the same action as the taskbar. */
#define IC_TERM_X 56
#define IC_TERM_Y 64
#define IC_FILE_X 56
#define IC_FILE_Y 150
#define IC_SIZE 48

static struct {
    u8 open;
    u16 x, y, w, h;
} term = {0, 24, 24, 560, 344};

static struct {
    u8 open;
    u16 x, y, w, h;
} flsw = {0, 608, 24, 168, 300};

static int gui_on = 0;
static int mx, my;
static u8 prev_left = 0;
static int hover = -1;

/* Window dragging: 0 = idle, 1 = terminal, 2 = files window. */
static int drag_win = 0;
static int drag_offx, drag_offy;

/* ---- Forward declarations --------------------------------------------- */

static void redraw_scene(void);
static void open_term(void);
static void close_term(void);
static void open_files(void);
static void close_files(void);
static void draw_files_win(void);

/* ---- Mouse cursor ----------------------------------------------------- */

#define CUR_W 8
#define CUR_H 12
static const unsigned char cur_spr[CUR_H] = {0x80, 0xC0, 0xE0, 0xF0, 0xF8,
                                             0xFC, 0xFE, 0xFF, 0xF8, 0xD8,
                                             0x98, 0x18};
/* Backing store for the (CUR_W+1)x(CUR_H+1) region under the cursor
 * (sprite + drop shadow), so erasing never needs a window repaint. */
#define CUR_BW (CUR_W + 1)
#define CUR_BH (CUR_H + 1)
static u32 cur_bg[CUR_BW * CUR_BH];
static void cursor_save(void) {
    for (u32 r = 0; r < CUR_BH; r++) {
        volatile u32 *p =
            (volatile u32 *)(FB_LFB + ((my + r) * FB_WIDTH + mx) * 4);
        for (u32 c = 0; c < CUR_BW; c++) cur_bg[r * CUR_BW + c] = p[c];
    }
}

static void cursor_restore(void) {
    for (u32 r = 0; r < CUR_BH; r++) {
        volatile u32 *p =
            (volatile u32 *)(FB_LFB + ((my + r) * FB_WIDTH + mx) * 4);
        for (u32 c = 0; c < CUR_BW; c++) p[c] = cur_bg[r * CUR_BW + c];
    }
}

static void cursor_draw(void) {
    /* Black drop shadow at (+1,+1), white arrow on top. */
    for (int pass = 0; pass < 2; pass++) {
        u32 col = pass ? 0x00FFFFFF : 0x00000000;
        int ox = mx + (pass ? 0 : 1);
        int oy = my + (pass ? 0 : 1);
        for (int r = 0; r < CUR_H; r++) {
            unsigned char bits = cur_spr[r];
            if (!bits) continue;
            int py = oy + r;
            if (py < 0 || py >= (int)FB_HEIGHT) continue;
            volatile u32 *p =
                (volatile u32 *)(FB_LFB + (py * FB_WIDTH) * 4);
            for (int c = 0; c < CUR_W; c++) {
                if (!((bits >> (7 - c)) & 1)) continue;
                int px = ox + c;
                if (px < 0 || px >= (int)FB_WIDTH) continue;
                p[px] = col;
            }
        }
    }
}

/* ---- Small helpers ---------------------------------------------------- */

static int in_rect(int x, int y, int rx, int ry, int rw, int rh) {
    return x >= rx && x < rx + rw && y >= ry && y < ry + rh;
}

static void draw_label_fit(u32 x, u32 y, const char *s, u32 max_chars,
                           u32 fg) {
    char buf[21];
    u32 i = 0;
    while (s[i] && i < max_chars && i < sizeof(buf) - 1) {
        buf[i] = s[i];
        i++;
    }
    buf[i] = '\0';
    fb_draw_string(x, y, buf, fg);
}

/* ---- Windows ----------------------------------------------------------- */

static void term_content_rect(int *cx, int *cy, int *cw, int *ch) {
    *cx = term.x + 2;
    *cy = term.y + 22;
    *cw = term.w - 4;
    *ch = term.h - 24;
}

/* Thin position indicator on the right edge of the terminal content,
 * shown only while there is history beyond the visible rows. */
static void draw_term_scrollbar(void) {
    if (!term.open) return;
    int cx, cy, cw, ch;
    term_content_rect(&cx, &cy, &cw, &ch);
    int total = fb_console_history_lines();
    int vis = fb_console_view_rows();
    if (total <= vis) return;
    int off = fb_console_scroll_off();
    int maxoff = fb_console_max_scroll();
    int th = ch * vis / total;
    if (th < 8) th = 8;
    int ty = cy + (ch - th) * off / maxoff;
    fb_fillrect(cx + cw - 4, cy, 4, ch, FB_CONSOLE_BG);
    fb_fillrect(cx + cw - 4, ty, 4, th, 0x003288A8);
}

static void draw_window_chrome(u16 x, u16 y, u16 w, u16 h,
                               const char *title) {
    fb_fillrect(x, y, w, h, WIN_LINE);      /* border */
    fb_fillrect(x + 2, y + 20, w - 4, h - 22, WIN_BG);
    fb_fillrect(x + 2, y + 2, w - 4, 18, WIN_TITLE);
    fb_draw_string(x + 8, y + 5, title, 0x00FFFFFF);
    /* Close box */
    fb_fillrect(x + w - 16, y + 4, 13, 14, 0x00A83232);
    fb_draw_string(x + w - 13, y + 5, "X", 0x00FFFFFF);
}

static void open_term(void) {
    int cx, cy, cw, ch;
    term.open = 1;
    draw_window_chrome(term.x, term.y, term.w, term.h, "TERMINAL");
    term_content_rect(&cx, &cy, &cw, &ch);
    fb_fillrect(cx, cy, cw, ch, FB_CONSOLE_BG);
    fb_console_set_viewport(cx, cy, cw, ch);
    kprint("> ");
    /* Window paints wipe the sprite; put the cursor back on top. */
    cursor_save();
    cursor_draw();
}

static void close_term(void) {
    term.open = 0;
    fb_console_disable();
    redraw_scene();
}

static void open_files(void) {
    flsw.open = 1;
    draw_files_win();
    cursor_save();
    cursor_draw();
}

static void close_files(void) {
    flsw.open = 0;
    redraw_scene();
}

static void draw_files_win(void) {
    draw_window_chrome(flsw.x, flsw.y, flsw.w, flsw.h, "FILES");
    int n = files_count();
    int rows = (flsw.h - 26 - 2) / 16;
    if (n == 0) {
        fb_draw_string(flsw.x + 8, flsw.y + 28, "(no files)", 0x0090B8C8);
        return;
    }
    for (int i = 0; i < n && i < rows; i++) {
        draw_label_fit(flsw.x + 8, flsw.y + 28 + i * 16, files_get(i),
                       (flsw.w - 16) / 8, 0x00EAEAEA);
    }
}

/* Click on file row i: dump its content into the terminal. */
static void files_open_row(int i) {
    if (i < 0 || i >= files_count()) return;
    if (!term.open) open_term();
    const char *name = files_get(i);
    const char *data = files_get_content(i);
    kprint(name);
    kprint(": ");
    kprint(data ? data : "");
    kprint("\n> ");
}

/* ---- Desktop ----------------------------------------------------------- */

static void draw_icons(void) {
    /* Terminal icon: teal tile with a ">_" prompt glyph. */
    fb_fillrect(IC_TERM_X, IC_TERM_Y, IC_SIZE, IC_SIZE, ICON_TERM);
    fb_draw_string(IC_TERM_X + 8, IC_TERM_Y + 8, ">_", 0x00FFFFFF);
    fb_draw_string(IC_TERM_X - 8, IC_TERM_Y + IC_SIZE + 4, "TERMINAL",
                   0x00D0E8F0);

    /* Files icon: orange tile with document lines. */
    fb_fillrect(IC_FILE_X, IC_FILE_Y, IC_SIZE, IC_SIZE, ICON_FILE);
    for (int i = 0; i < 4; i++)
        fb_fillrect(IC_FILE_X + 8, IC_FILE_Y + 10 + i * 8, 32, 3,
                    0x00FFFFFF);
    fb_draw_string(IC_FILE_X + 4, IC_FILE_Y + IC_SIZE + 4, "FILES",
                   0x00D0E8F0);
}

static void draw_button(const btn_t *b, int hovered) {
    fb_fillrect(b->x, b->y, b->w, b->h, hovered ? BTN_HOVER : BTN_BG);
    fb_fillrect(b->x, b->y, b->w, 1, hovered ? 0x0050A8C4 : WIN_LINE);
    u32 tw = strlen((char *)b->label) * 8;
    fb_draw_string(b->x + (b->w - tw) / 2, b->y + 4, b->label, BTN_FG);
}

static void draw_taskbar(void) {
    fb_fillrect(0, TB_Y, FB_WIDTH, TB_H, BAR_BG);
    fb_fillrect(0, TB_Y, FB_WIDTH, 1, BAR_LINE);
    for (int i = 0; i < BTN_COUNT; i++)
        draw_button(&btns[i], i == hover);
}

/* Repaint the whole desktop: background, icons, windows (with live
 * console content re-rendered from the scrollback buffer), taskbar,
 * cursor last. */
static void redraw_scene(void) {
    cursor_restore();
    fb_clear(DESK_BG);
    draw_icons();
    if (flsw.open) draw_files_win();
    if (term.open) {
        int cx, cy, cw, ch;
        draw_window_chrome(term.x, term.y, term.w, term.h, "TERMINAL");
        term_content_rect(&cx, &cy, &cw, &ch);
        fb_console_move_viewport(cx, cy, cw, ch);
        draw_term_scrollbar();
    }
    draw_taskbar();
    cursor_save();
    cursor_draw();
}

/* ---- Actions ------------------------------------------------------------ */

static void action(u8 id) {
    switch (id) {
    case B_TERMINAL:
        term.open ? close_term() : open_term();
        break;
    case B_FILES:
        flsw.open ? close_files() : open_files();
        break;
    case B_REBOOT:
        serial_write_str("gui:reboot\n");
        power_reboot();
        break;
    case B_SHUTDOWN:
        serial_write_str("gui:shutdown\n");
        power_shutdown();
        break;
    }
}

/* Returns 1 when the click was consumed by a widget. */
static int on_click(int x, int y) {
    /* Window close boxes first (topmost). */
    if (term.open && in_rect(x, y, term.x + term.w - 16, term.y + 4, 13,
                             14)) {
        close_term();
        return 1;
    }
    if (flsw.open &&
        in_rect(x, y, flsw.x + flsw.w - 16, flsw.y + 4, 13, 14)) {
        close_files();
        return 1;
    }

    /* File rows in the explorer window (below its title bar, which is
     * left unconsumed so windows can be dragged from it). */
    if (flsw.open &&
        in_rect(x, y, flsw.x, flsw.y + 24, flsw.w, flsw.h - 24)) {
        int row = (y - (flsw.y + 28)) / 16;
        if (row >= 0) files_open_row(row);
        return 1;
    }

    /* Taskbar buttons. */
    for (int i = 0; i < BTN_COUNT; i++) {
        if (in_rect(x, y, btns[i].x, btns[i].y, btns[i].w, btns[i].h)) {
            action(i);
            return 1;
        }
    }

    /* Desktop icons. */
    if (in_rect(x, y, IC_TERM_X, IC_TERM_Y, IC_SIZE, IC_SIZE)) {
        action(B_TERMINAL);
        return 1;
    }
    if (in_rect(x, y, IC_FILE_X, IC_FILE_Y, IC_SIZE, IC_SIZE)) {
        action(B_FILES);
        return 1;
    }
    return 0;
}

/* Grab a window by its title bar (the close box is checked first in
 * on_click, so a hit here means the rest of the band). */
static void try_begin_drag(int x, int y) {
    if (term.open && in_rect(x, y, term.x + 2, term.y + 2, term.w - 4,
                             18)) {
        drag_win = 1;
        drag_offx = x - term.x;
        drag_offy = y - term.y;
    } else if (flsw.open &&
               in_rect(x, y, flsw.x + 2, flsw.y + 2, flsw.w - 4, 18)) {
        drag_win = 2;
        drag_offx = x - flsw.x;
        drag_offy = y - flsw.y;
    }
}

/* Move the dragged window to a new top-left corner, clamped so its
 * title bar stays reachable, then repaint the scene. */
static void drag_to(int nx, int ny) {
    u16 *x = (drag_win == 1) ? &term.x : &flsw.x;
    u16 *y = (drag_win == 1) ? &term.y : &flsw.y;
    u16 w = (drag_win == 1) ? term.w : flsw.w;

    if (nx < -(int)w + 60) nx = -(int)w + 60;
    if (nx > (int)FB_WIDTH - 60) nx = (int)FB_WIDTH - 60;
    if (ny < 0) ny = 0;
    if (ny > TB_Y - 22) ny = TB_Y - 22;


    if ((int)*x != nx || (int)*y != ny) {
        *x = (u16)nx;
        *y = (u16)ny;
        redraw_scene();
    }
}

/* ---- Public API ---------------------------------------------------------- */

void gui_init(void) {
    mx = FB_WIDTH / 2;
    my = FB_HEIGHT / 2;
    prev_left = 0;
    hover = -1;
    gui_on = 1;
    redraw_scene();
    open_term();
}

int gui_active(void) { return gui_on; }

void gui_update(void) {
    if (!gui_on) return;

    /* The vmmouse signals packets through the i8042 aux line, which
     * stays silent because the PS/2 mouse is never enabled in this
     * mode; poll the backdoor queue directly instead. */
    if (mouse_absolute()) vmmouse_poll();

    int dx, dy, dz;
    mouse_drain(&dx, &dy, &dz);

    /* Wheel: the device reports dz<0 for wheel up/away, which reveals
     * older output. */
    if (dz && term.open) {
        fb_console_scroll(-3 * dz);
        draw_term_scrollbar();
    }

    int moved = 0;
    if (mouse_absolute()) {
        /* vmmouse: mirror the host pointer position directly. */
        int nx = ((int)mouse_abs_x() * FB_WIDTH) >> 16;
        int ny = ((int)mouse_abs_y() * FB_HEIGHT) >> 16;
        if (nx > (int)FB_WIDTH - CUR_BW) nx = FB_WIDTH - CUR_BW;
        if (ny > (int)FB_HEIGHT - CUR_BH) ny = FB_HEIGHT - CUR_BH;
        if (nx != mx || ny != my) {
            cursor_restore();
            mx = nx;
            my = ny;
            moved = 1;
        }
    } else {
        moved = dx || dy;
        if (moved) {
            cursor_restore();
            mx += dx;
            my += dy;
            if (mx < 0) mx = 0;
            if (my < 0) my = 0;
            if (mx > (int)FB_WIDTH - CUR_BW) mx = FB_WIDTH - CUR_BW;
            if (my > (int)FB_HEIGHT - CUR_BH) my = FB_HEIGHT - CUR_BH;
        }
    }

    /* Hover feedback on taskbar buttons. */
    int h = -1;
    for (int i = 0; i < BTN_COUNT; i++) {
        if (in_rect(mx, my, btns[i].x, btns[i].y, btns[i].w, btns[i].h)) {
            h = i;
            break;
        }
    }
    int hover_changed = h != hover;
    hover = h;

    if (hover_changed) draw_taskbar();

    /* Repaints wipe the sprite when it overlaps them; refresh it then,
     * and always after a move (fresh backing store at the new spot). */
    if (moved ||
        (hover_changed && in_rect(mx, my, 0, TB_Y, FB_WIDTH, TB_H))) {
        cursor_save();
        cursor_draw();
    }

    u8 left = mouse_buttons() & 0x01;
    if (drag_win) {
        /* Settle at the current position even on release: packets
         * arrive in bursts, so the final moves often share a tick
         * with the release and would otherwise be skipped. */
        drag_to(mx - drag_offx, my - drag_offy);
        if (!left) drag_win = 0; /* button released: drop the window */
    } else {
        /* In absolute mode a press is latched by the driver, because
         * one drain pass can cover a whole press+release pair. */
        int clicked = 0;
        int cx = mx, cy = my;
        if (mouse_absolute()) {
            u16 ax, ay;
            if (mouse_take_click(&ax, &ay)) {
                /* device coords -> screen coords, like the cursor */
                cx = ((int)ax * FB_WIDTH) >> 16;
                cy = ((int)ay * FB_HEIGHT) >> 16;
                clicked = 1;
            }
        } else if (left && !prev_left) {
            clicked = 1;
        }
        if (clicked && !on_click(cx, cy)) try_begin_drag(cx, cy);
    }
    prev_left = left;
}

void gui_refresh(void) {
    if (!gui_on) return;
    if (flsw.open) draw_files_win();
    draw_term_scrollbar();
    cursor_save();
    cursor_draw();
}

void gui_logout(void) {
    if (!gui_on) return;
    term.open = 0;
    flsw.open = 0;
    fb_console_disable();
    redraw_scene();
}
