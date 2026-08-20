#ifndef GDT_H
#define GDT_H

#include "types.h"

/* Segment selectors (matching the order built in gdt.c):
 * 0x08 kernel code, 0x10 kernel data, 0x18 user code, 0x20 user data,
 * 0x28 TSS. User selectors are used with RPL 3 (0x1B / 0x23). */
#define GDT_KERNEL_CS 0x08
#define GDT_KERNEL_DS 0x10
#define GDT_USER_CS   0x18
#define GDT_USER_DS   0x20
#define GDT_TSS_SEL   0x28

/* Rebuilds the boot GDT in C, adds ring-3 segments and a TSS,
 * then reloads GDTR, the segment registers and the task register. */
void gdt_init(void);

/* TSS.esp0 is loaded by the CPU on every ring-3 -> ring-0 transition;
 * the scheduler must point it at the current task's kernel stack. */
void tss_set_esp0(u32 esp0);

#endif