#include "power.h"
#include "../cpu/ports.h"

void power_reboot(void) {
    asm volatile("cli");
    /* Pulse the CPU reset line through the 8042 controller. */
    for (int i = 0; i < 100000 && (port_byte_in(0x64) & 0x02); i++)
        ;
    port_byte_out(0x64, 0xFE);
    for (;;)
        asm volatile("hlt"); /* not reached on real reset */
}

void power_shutdown(void) {
    asm volatile("cli");
    /* S5 sleep via PM1a_CNT. 0x604 is the port QEMU's PIIX4 exposes;
     * the other two cover Bochs and VirtualBox as harmless fallbacks. */
    port_word_out(0x604, 0x2000);
    port_word_out(0xB004, 0x2000);
    port_word_out(0x4004, 0x2000);
    for (;;)
        asm volatile("hlt");
}
