#include "syscall.h"
#include "task.h"
#include "../drivers/screen.h"
#include "../drivers/serial.h"
#include "../drivers/framebuffer.h"

#define SYS_WRITE     1
#define SYS_EXIT      2
#define SYS_YIELD     3
#define SYS_FB_INFO   4
#define SYS_FB_PT     5
#define SYS_FB_RECT   6
#define SYS_FB_CLEAR  7

/* Ring-3 -> ring-0 syscall ABI: u32 args arrive in ebx/ecx/edx, pointer
 * args reference the caller's own (flat, unmapped) memory. */

static void sys_write(const char *s) {
    if (s) kprint(s);
}

static void sys_fb_info(u32 *out) {
    /* write only when a valid pointer is supplied */
    u32 w, h, bpp;
    fb_info(&w, &h, &bpp);
    if (out) { out[0] = w; out[1] = h; out[2] = bpp; }
}

typedef struct {
    u32 x, y, w, h, rgb;
} fb_rect_t;

void syscall_dispatch(registers_t r) {
    switch (r.eax) {
    case SYS_WRITE:
        sys_write((const char *)r.ebx);
        break;
    case SYS_EXIT:
        task_exit();
        break;
    case SYS_YIELD:
        schedule();
        break;
    case SYS_FB_INFO:
        sys_fb_info((u32 *)r.ebx);
        break;
    case SYS_FB_PT:
        fb_putpixel(r.ebx, r.ecx, r.edx);
        break;
    case SYS_FB_RECT: {
        fb_rect_t *p = (fb_rect_t *)r.ebx;
        if (p) fb_fillrect(p->x, p->y, p->w, p->h, p->rgb);
        break;
    }
    case SYS_FB_CLEAR:
        fb_clear(r.ebx);
        break;
    default:
        serial_write_str("syscall:unknown ");
        serial_write_int((int)r.eax);
        serial_write_str("\n");
        task_exit();
        break;
    }
}