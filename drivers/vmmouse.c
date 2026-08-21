#include "vmmouse.h"
#include "mouse.h"
#include "../cpu/isr.h"
#include "../libc/function.h"
#include "serial.h"

/* The vmmouse lives behind the VMware backdoor port. Calls pass
 * arguments in registers: EAX must hold the magic, ECX the command,
 * EBX an optional argument; results come back in EAX..EDX.
 *
 * Packet flow: QEMU queues [buttons, x, y, z] words and raises IRQ12
 * (piggybacking on the i8042 aux line) whenever new data arrives; we
 * then poll the status/data ports from the interrupt handler. */

#define VMPORT_PORT            0x5658
#define VMPORT_MAGIC           0x564D5868u

#define VMPORT_CMD_GETVERSION      10
#define VMPORT_CMD_VMMOUSE_DATA    39
#define VMPORT_CMD_VMMOUSE_STATUS  40
#define VMPORT_CMD_VMMOUSE_COMMAND 41

#define VMMOUSE_READ_ID          0x45414552u
#define VMMOUSE_REQUEST_RELATIVE 0x4c455252u
#define VMMOUSE_REQUEST_ABSOLUTE 0x53424152u

#define VMMOUSE_LEFT   0x20
#define VMMOUSE_RIGHT  0x10
#define VMMOUSE_MIDDLE 0x08

/* Backdoor call: all four registers are both inputs and outputs. */
static void vmport_call(u32 *a, u32 *b, u32 *c, u32 *d) {
    asm volatile("inl %%dx, %%eax"
                 : "+a"(*a), "+b"(*b), "+c"(*c), "+d"(*d)
                 :
                 : "memory");
}

void vmmouse_poll(void) {
    for (;;) {
        u32 a = VMPORT_MAGIC, b = 0, c = VMPORT_CMD_VMMOUSE_STATUS,
            d = VMPORT_PORT;
        vmport_call(&a, &b, &c, &d);
        if ((a >> 16) != 0 || (a & 0xFFFF) < 4) return;

        a = VMPORT_MAGIC; /* every backdoor call restates the magic */
        b = 4;
        c = VMPORT_CMD_VMMOUSE_DATA;
        d = VMPORT_PORT;
        vmport_call(&a, &b, &c, &d);

        u8 raw = (u8)a;
        u8 btns = (raw & VMMOUSE_LEFT ? 0x01 : 0) |
                  (raw & VMMOUSE_RIGHT ? 0x02 : 0) |
                  (raw & VMMOUSE_MIDDLE ? 0x04 : 0);
        mouse_push_abs((u16)b, (u16)c, btns, (int)d);
    }
}

static void vmmouse_callback(registers_t regs) {
    UNUSED(regs);
    vmmouse_poll();
}

int vmmouse_init(void) {
    /* Detect the backdoor: GETVERSION answers with EBX = magic. */
    u32 a = VMPORT_MAGIC, b = 0, c = VMPORT_CMD_GETVERSION,
        d = VMPORT_PORT;
    vmport_call(&a, &b, &c, &d);
    if (b != VMPORT_MAGIC) return 0;

    register_interrupt_handler(IRQ12, vmmouse_callback);

    /* READ_ID clears the device's disabled status (it boots shut) and
     * makes it register its input handler; it also queues one version
     * word, drained below to keep packet alignment. */
    a = VMPORT_MAGIC;
    b = VMMOUSE_READ_ID;
    c = VMPORT_CMD_VMMOUSE_COMMAND;
    d = VMPORT_PORT;
    vmport_call(&a, &b, &c, &d);

    a = VMPORT_MAGIC;
    b = VMMOUSE_REQUEST_ABSOLUTE;
    c = VMPORT_CMD_VMMOUSE_COMMAND;
    d = VMPORT_PORT;
    vmport_call(&a, &b, &c, &d);

    /* Discard the queued version word so reads start on a packet. */
    for (;;) {
        a = VMPORT_MAGIC;
        b = 0;
        c = VMPORT_CMD_VMMOUSE_STATUS;
        d = VMPORT_PORT;
        vmport_call(&a, &b, &c, &d);
        if ((a >> 16) != 0 || (a & 0xFFFF) == 0) break;
        a = VMPORT_MAGIC; /* every backdoor call restates the magic */
        b = 1;
        c = VMPORT_CMD_VMMOUSE_DATA;
        d = VMPORT_PORT;
        vmport_call(&a, &b, &c, &d);
    }
    return 1;
}
