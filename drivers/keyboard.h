#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "../cpu/types.h"

void init_keyboard();

/* Input consumed by the shell task. The IRQ handler only fills the
 * buffer; the shell polls input_pending() and drains it in task
 * context so commands never run inside an interrupt. */
int input_pending(void);
char *input_line(void);
void input_consume(void);

#endif