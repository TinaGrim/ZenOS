typedef unsigned int u32;

/* Standard syscall wrapper: eax=number, ebx/ecx/edx=args, fixed registers
 * via constraints instead of relying on compiler-chosen stack args. */
static u32 syscall(u32 nr, u32 a, u32 b, u32 c) {
    asm volatile("int $0x80"
                 : "+a"(nr)
                 : "b"(a), "c"(b), "d"(c)
                 : "memory");
    return nr;
}

typedef struct {
    u32 x, y, w, h, rgb;
} fb_rect_t;

/* Entry point. Placed in .text.entry so the linker script puts it at the
 * very start of the binary (0x100000); the kernel jumps straight here.
 * Any code emitted before it would be executed first, so _start MUST be
 * the first byte of the flat image. Prints a few lines, launches a small
 * framebuffer demo (color bands + bouncing box), then runs forever. */
__attribute__((section(".text.entry"), noreturn)) void _start(void) {
    int n;
    fb_rect_t r;

    for (n = 0; n < 3; n++)
        syscall(1, (u32)"HELLO FROM USER MODE\n", 0, 0);

    /* Snapshot the framebuffer geometry via the fb_info syscall. */
    u32 info[3];
    syscall(4, (u32)info, 0, 0);
    u32 w = info[0];
    u32 h = info[1];
    u32 band = w / 8;

    /* Bouncing checker box: repaint background bands, draw the box, then
     * yield so the timer can time-slice between this task and the shell.
     * The bands are redrawn every frame because the clear would erase
     * them otherwise. */
    static const u32 pal[8] = {0x00FF0000, 0x00FF7F00, 0x00FFFF00, 0x0000FF00,
                               0x0000FFFF, 0x000000FF, 0x008B00FF, 0x00FF00FF};
    int bx = 0, by = 0, dx = 6, dy = 4;
    for (;;) {
        syscall(7, 0x00101820, 0, 0);

        for (n = 0; n < 8; n++) {
            r.x = (u32)n * band;
            r.y = 0;
            r.w = band;
            r.h = h;
            r.rgb = pal[n];
            syscall(6, (u32)&r, 0, 0);
        }

        r.x = (u32)bx;
        r.y = (u32)by;
        r.w = 40;
        r.h = 40;
        r.rgb = 0x00FFCC00;
        syscall(6, (u32)&r, 0, 0);

        r.x = (u32)bx + 12;
        r.y = (u32)by + 12;
        r.w = 16;
        r.h = 16;
        r.rgb = 0x00000000;
        syscall(6, (u32)&r, 0, 0);

        syscall(3, 0, 0, 0);   /* yield */

        bx += dx;
        by += dy;
        if (bx + 40 >= (int)w) { bx = (int)w - 40; dx = -6; }
        if (by + 40 >= (int)h) { by = (int)h - 40; dy = -4; }
        if (bx <= 0) { bx = 0; dx = 6; }
        if (by <= 0) { by = 0; dy = 4; }
    }
}