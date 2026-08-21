#ifndef VMMOUSE_H
#define VMMOUSE_H

#include "../cpu/types.h"

/* VMware backdoor absolute mouse ("vmmouse"), emulated by QEMU's pc
 * machine whenever vmport is enabled (the default). Reports absolute
 * screen coordinates, so the guest cursor tracks the host pointer
 * one-to-one instead of accumulating relative deltas. */

/* Probe the backdoor and switch the device to absolute reporting.
 * Returns 1 on success; on failure the PS/2 mouse stays in charge. */
int vmmouse_init(void);

/* Drain all queued absolute packets into the mouse state. QEMU only
 * signals new packets through the i8042 aux line while the PS/2 mouse
 * is enabled, which the vmmouse path never does — so consumers poll. */
void vmmouse_poll(void);

#endif
