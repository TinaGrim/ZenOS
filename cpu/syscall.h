#ifndef SYSCALL_H
#define SYSCALL_H

#include "isr.h"

/* int 0x80 dispatcher. Syscall number in eax, args in ebx/ecx/edx. */
void syscall_dispatch(registers_t r);

#endif