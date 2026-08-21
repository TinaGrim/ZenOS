#ifndef POWER_H
#define POWER_H

/* Reboot via the 8042 keyboard controller (does not return) and ACPI
 * poweroff through the PIIX4 PM register QEMU exposes at 0x604. */

void power_reboot(void);
void power_shutdown(void);

#endif
