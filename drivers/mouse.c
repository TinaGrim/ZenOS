#include "mouse.h"
#include "vmmouse.h"
#include "../cpu/isr.h"
#include "../cpu/ports.h"
#include "../libc/function.h"
#include "serial.h"

/* Standard PS/2 mouse: commands go to the controller (0x64) tagged with
 * 0xD4, data both ways flows through 0x60. In streaming mode the device
 * emits packets on every motion/sample:
 *   b0: Yovf Xovf Ysign Xsign 1 mid right left
 *   b1: x delta (signed byte)
 *   b2: y delta (signed byte, positive = up)
 *   b3: wheel delta, low nibble signed (Intellimouse mode only)
 *
 * Incoming bytes are pushed into a queue and frames are parsed from its
 * head; any byte that cannot start a frame is dropped and parsing
 * retries, so the stream self-heals after desyncs instead of latching
 * onto a bad phase ("mouse sync does not match"). */

#define MOUSE_LEFT  0x01
#define MOUSE_RIGHT 0x02

static int acc_dx = 0;
static int acc_dy = 0;
static int acc_dz = 0;
static u8 buttons = 0;
static int has_wheel = 0;

/* Absolute mode (vmmouse): the latest host pointer position, 0..FFFF
 * scaled to the screen by the consumer. */
static u16 abs_x = 0;
static u16 abs_y = 0;
static int absolute_mode = 0;

/* Press edges are recorded as they arrive because several packets can
 * be drained in a single gui_update pass; a press+release pair that
 * lands inside one pass must still yield one click. */
static u8 click_pending = 0;
static u16 click_x, click_y;

void mouse_set_absolute(int on) { absolute_mode = on; }

int mouse_absolute(void) { return absolute_mode; }

u16 mouse_abs_x(void) { return abs_x; }

u16 mouse_abs_y(void) { return abs_y; }

/* Called by the vmmouse IRQ handler with a fresh absolute sample. */
void mouse_push_abs(u16 x, u16 y, u8 btns, int dz) {
    if ((btns & 0x01) && !(buttons & 0x01)) {
        click_pending = 1;
        click_x = x;
        click_y = y;
    }
    abs_x = x;
    abs_y = y;
    buttons = btns;
    acc_dz += dz;
}

/* Consume a pending left-button press; returns 1 and fills the press
 * position (device coordinates) when one was seen since last call. */
int mouse_take_click(u16 *x, u16 *y) {
    if (!click_pending) return 0;
    *x = click_x;
    *y = click_y;
    click_pending = 0;
    return 1;
}

#define AUXQ_SZ 64
static volatile u8 auxq[AUXQ_SZ];
static volatile u8 auxq_len = 0;

static void auxq_push(u8 b) {
    if (auxq_len >= AUXQ_SZ) {
        /* Overflow means bytes were lost upstream; flush so parsing
         * restarts from a clean resync scan instead of a stale tail. */
        auxq_len = 0;
    }
    auxq[auxq_len++] = b;
}

static void auxq_pop(u8 n) {
    for (u8 i = n; i < auxq_len; i++) auxq[i - n] = auxq[i];
    auxq_len -= n;
}

static void mouse_write(u8 cmd) {
    /* Wait for the controller input buffer to drain, then send a
     * mouse-tagged command byte followed by the command itself. */
    for (int i = 0; i < 100000 && (port_byte_in(0x64) & 0x02); i++)
        ;
    port_byte_out(0x64, 0xD4);
    for (int i = 0; i < 100000 && (port_byte_in(0x64) & 0x02); i++)
        ;
    port_byte_out(0x60, cmd);
}

static u8 mouse_read_ack(void) {
    for (int i = 0; i < 100000; i++) {
        if (port_byte_in(0x64) & 0x01)
            return port_byte_in(0x60);
    }
    return 0xFF;
}

static void wait_ibf_clear(void) {
    for (int i = 0; i < 100000 && (port_byte_in(0x64) & 0x02); i++)
        ;
}

static void parse_frames(void);

static void mouse_callback(registers_t regs) {
    UNUSED(regs);
    /* Queue everything pending; only bytes flagged as auxiliary data
     * belong to the packet stream (the keyboard shares port 0x60). */
    while (port_byte_in(0x64) & 0x01) {
        u8 status = port_byte_in(0x64);
        u8 data = port_byte_in(0x60);
        if (!(status & 0x20)) continue; /* keyboard byte, not ours */
        auxq_push(data);
    }
    parse_frames();
}

/* Parse whole frames off the queue head. Byte 0 of a valid frame always
 * has bit 3 set and never carries the overflow bits; in wheel mode the
 * wheel byte's high nibble is all 0s or all 1s. Any head byte that
 * cannot start a frame is dropped and parsing retries, so a mid-stream
 * desync resynchronises instead of corrupting every packet (and instead
 * of hallucinating button presses out of delta bytes). */
static void parse_frames(void) {
    u8 fsz = has_wheel ? 4 : 3;
    while (auxq_len >= fsz) {
        u8 b0 = auxq[0];
        if (!(b0 & 0x08) || (b0 & 0xC0)) {
            auxq_pop(1);
            continue;
        }
        u8 b3 = has_wheel ? auxq[3] : 0;
        if (has_wheel && (b3 & 0xF0) != 0x00 && (b3 & 0xF0) != 0xF0) {
            auxq_pop(1);
            continue;
        }
        int dx = (int)(s8)auxq[1];
        /* PS/2 dy is positive when moving up; screen y grows down, so
         * invert before accumulating. */
        int dy = -(int)(s8)auxq[2];
        int dz = has_wheel ? (((int)(b3 & 0x0F) << 28) >> 28) : 0;
        auxq_pop(fsz);

        buttons = b0 & 0x07;
        acc_dx += dx;
        acc_dy += dy;
        acc_dz += dz;
    }
}

void init_mouse(void) {
    /* Prefer the VMware absolute mouse: it reports true host pointer
     * positions instead of relative deltas. Falls back to plain PS/2
     * when the backdoor is absent (real hardware). */
    if (vmmouse_init()) {
        mouse_set_absolute(1);
        serial_write_str("mouse:vmmouse-absolute\n");
        /* The vmmouse signals new packets through the i8042 aux line,
         * so the controller and PIC still need the same setup. */
        port_byte_out(0x64, 0xA8);
        wait_ibf_clear();
        port_byte_out(0x64, 0x20);
        u8 cfg = 0xFF;
        for (int i = 0; i < 100000; i++)
            if (port_byte_in(0x64) & 0x01) { cfg = port_byte_in(0x60); break; }
        cfg |= 0x02;
        cfg &= (u8)~0x20;
        wait_ibf_clear();
        port_byte_out(0x64, 0x60);
        wait_ibf_clear();
        port_byte_out(0x60, cfg);
        port_byte_out(0xA1, port_byte_in(0xA1) & ~0x10);
        return;
    }

    register_interrupt_handler(IRQ12, mouse_callback);

    /* Flush any stale output bytes. */
    for (int i = 0; i < 100000 && (port_byte_in(0x64) & 0x01); i++)
        port_byte_in(0x60);

    port_byte_out(0x64, 0xA8); /* enable auxiliary device */

    /* Rewrite the controller config: enable IRQ12 (bit 1), enable the
     * mouse clock (clear bit 5). Without this the controller swallows
     * packets and never raises the interrupt. */
    wait_ibf_clear();
    port_byte_out(0x64, 0x20);
    u8 cfg = 0xFF;
    for (int i = 0; i < 100000; i++)
        if (port_byte_in(0x64) & 0x01) { cfg = port_byte_in(0x60); break; }
    cfg |= 0x02;
    cfg &= (u8)~0x20;
    wait_ibf_clear();
    port_byte_out(0x64, 0x60);
    wait_ibf_clear();
    port_byte_out(0x60, cfg);

    mouse_write(0xF6); /* set defaults */
    mouse_read_ack();

    /* Intellimouse magic: three sample-rate writes (200, 100, 80)
     * switch the device into wheel mode; the ID then reads back as 3
     * and packets grow to 4 bytes. Harmless no-op on plain devices. */
    static const u8 rates[3] = {200, 100, 80};
    for (int i = 0; i < 3; i++) {
        mouse_write(0xF3);
        mouse_read_ack();
        mouse_write(rates[i]);
        mouse_read_ack();
    }
    mouse_write(0xF2); /* read device id */
    mouse_read_ack();
    u8 id = mouse_read_ack();
    has_wheel = (id == 3 || id == 4);
    serial_write_str("mouse:id=");
    serial_write_int(id);
    serial_write_str(" wheel=");
    serial_write_int(has_wheel);
    serial_write_str("\n");

    mouse_write(0xF4); /* enable data reporting */
    mouse_read_ack();

    /* Unmask IRQ12 on the slave PIC (bit 4). */
    port_byte_out(0xA1, port_byte_in(0xA1) & ~0x10);
}

int mouse_dx(void) { return acc_dx; }
int mouse_dy(void) { return acc_dy; }
u8 mouse_buttons(void) { return buttons; }
int mouse_has_wheel(void) { return has_wheel; }

/* gui.c drains via this; it resets atomically w.r.t. the IRQ by
 * disabling interrupts around the read-clear. Deltas are clamped so a
 * backlog built up during long output bursts cannot teleport the
 * cursor. */
#define DRAIN_CLAMP 2048
static int clamp_d(int v) {
    if (v > DRAIN_CLAMP) return DRAIN_CLAMP;
    if (v < -DRAIN_CLAMP) return -DRAIN_CLAMP;
    return v;
}

void mouse_drain(int *dx, int *dy, int *dz) {
    asm volatile("cli");
    *dx = clamp_d(acc_dx);
    *dy = clamp_d(acc_dy);
    *dz = acc_dz;
    acc_dx = 0;
    acc_dy = 0;
    acc_dz = 0;
    asm volatile("sti");
}
