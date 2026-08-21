#ifndef MOUSE_H
#define MOUSE_H

#include "../cpu/types.h"

/* PS/2 mouse on IRQ12. init_mouse() enables the auxiliary port, tries
 * to switch the device into Intellimouse wheel mode (4-byte packets)
 * and starts streaming; the IRQ handler decodes packets into button
 * state and accumulated relative motion + wheel delta, drained by
 * gui_update(). */

void init_mouse(void);

/* Accumulated motion since the last drain (screen pixels). */
int mouse_dx(void);
int mouse_dy(void);

/* Live button bits: 1 = left, 2 = right, 4 = middle. */
u8 mouse_buttons(void);

/* Nonzero when the device answered the Intellimouse identification. */
int mouse_has_wheel(void);

/* Atomically read and clear the accumulated motion and wheel delta.
 * dz is in wheel notches: positive = wheel up/away from user. */
void mouse_drain(int *dx, int *dy, int *dz);

/* Absolute mode (VMware vmmouse): the cursor mirrors the host pointer
 * instead of accumulating relative deltas. */
int mouse_absolute(void);
u16 mouse_abs_x(void); /* 0..65535, scale to screen width */
u16 mouse_abs_y(void); /* 0..65535, scale to screen height */

/* Feed one absolute sample (vmmouse IRQ handler). */
void mouse_push_abs(u16 x, u16 y, u8 btns, int dz);

/* Consume a pending left-button press seen in absolute mode; returns 1
 * and fills the press position in device coordinates. Presses are
 * latched because a drain pass can swallow a press+release pair. */
int mouse_take_click(u16 *x, u16 *y);

#endif
