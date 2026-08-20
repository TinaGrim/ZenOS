#ifndef TASK_H
#define TASK_H

#include "types.h"

/* Task table limits; kernel stacks come from a bump allocator at 0x800000
 * and ring-3 stacks from a second arena at 0x700000. */
#define TASK_MAX 8
#define KSTACK_SIZE 0x4000
#define USER_STACK_SIZE 0x4000

typedef enum {
    TASK_FREE = 0,
    TASK_READY = 1,
    TASK_DEAD = 2,
    TASK_RUNNING = 3,
    TASK_WAITING = 4
} task_state_t;

typedef struct {
    u32 esp;             /* resume pointer: the ds slot of a saved stub frame */
    u32 kstack_top;      /* TSS.esp0 for this task */
    u32 user_stack_top;  /* ring-3 stack top (0 for ring-0 tasks) */
    u32 entry;
    u8  ring;
    task_state_t state;
} task_t;

extern task_t *current;

/* Set by the scheduler, consumed by task_epilogue in cpu/switch.asm:
 * the stub saves the interrupted task's esp into *switch_from, loads
 * switch_to and resumes there. */
extern volatile u8 need_switch;
extern u32 *switch_from;
extern u32 switch_to;

/* Build a fresh task (state READY). Returns pid, or -1 when the table
 * is full. Ring 0 tasks run kernel code, ring 3 tasks user code. */
int task_create(u32 entry, u8 ring);

/* Round-robin pick of the next READY task; no-op when nothing to do. */
void schedule(void);

/* Kill the running task and force a switch onto the next task. */
void task_exit(void);

/* Block the running task until task_wake() releases it. The task leaves
 * the ready pool; scheduler runs leave-run tasks alone instead. */
void task_wait(void);
void task_wake(u32 pid);

/* Bootstrap: create the shell task and enter it, never returning. */
void task_start_shell(u32 entry);

/* First context switch into the shell (cpu/switch.asm). */
void first_switch(u32 new_esp);

#endif