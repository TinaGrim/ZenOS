#ifndef KERNEL_H
#define KERNEL_H

#include "../cpu/types.h"

/* Preloaded demo user program, copied from the fixed sector range on the
 * disk (keep in sync with USER_BIN_SECTOR / -SECTORS in the makefile). */
#define USER_BIN_ADDR    0x100000
#define USER_BIN_SECTOR  100
#define USER_BIN_SECTORS 4

void user_input(char *input);
void shell_main(void);

#endif