#include "task.h"
#include "gdt.h"
#include "../drivers/serial.h"

static task_t tasks[TASK_MAX];
static u32 kstack_arena = 0x800000;
static u32 user_stack_arena = 0x700000;
static int ready_count = 0;

task_t *current = 0;
volatile u8 need_switch = 0;
u32 *switch_from = 0;
u32 switch_to = 0;

/* Lay out a fake interrupt frame on the task's kernel stack so the
 * shared stub epilogue (pop ds -> set segs -> popa -> add esp,8 -> iret)
 * resumes the task. Must mirror what the CPU + stub push, low to high:
 *   [ds][edi][esi][ebp][esp][ebx][edx][ecx][eax][int_no][err]
 *   [eip][cs][eflags][(+user esp, ss for ring 3)]
 * Ring-0 tasks get a 3-word iret frame, ring-3 tasks a 5-word one. */
static void build_frame(task_t *t) {
    u32 *s = (u32 *)t->kstack_top;

    if (t->ring == 3) {
        *(--s) = GDT_USER_DS | 3;        /* ss */
        *(--s) = t->user_stack_top;      /* user esp */
    }
    *(--s) = 0x202;                      /* eflags: IF, reserved bit 1 */
    *(--s) = (t->ring == 3) ? (GDT_USER_CS | 3) : GDT_KERNEL_CS;
    *(--s) = t->entry;                   /* eip */
    *(--s) = 0;                          /* err_code */
    *(--s) = 0;                          /* int_no */
    *(--s) = 0; *(--s) = 0; *(--s) = 0;  /* eax, ecx, edx */
    *(--s) = 0; *(--s) = 0;              /* ebx, saved esp */
    *(--s) = 0; *(--s) = 0;              /* ebp, esi */
    *(--s) = 0;                          /* edi */
    *(--s) = (t->ring == 3) ? (GDT_USER_DS | 3) : GDT_KERNEL_DS;

    t->esp = (u32)s;
}

int task_create(u32 entry, u8 ring) {
    for (int i = 0; i < TASK_MAX; i++) {
        if (tasks[i].state != TASK_FREE) continue;

        task_t *t = &tasks[i];
        t->state = TASK_READY;
        t->entry = entry;
        t->ring = ring;
        t->kstack_top = kstack_arena + KSTACK_SIZE;
        kstack_arena += KSTACK_SIZE;
        t->user_stack_top = 0;
        if (ring == 3) {
            t->user_stack_top = user_stack_arena + USER_STACK_SIZE;
            user_stack_arena += USER_STACK_SIZE;
        }
        build_frame(t);
        ready_count++;

        serial_write_str("task:create pid=");
        serial_write_int(i);
        serial_write_str(" ring=");
        serial_write_int(ring);
        serial_write_str("\n");
        return i;
    }
    serial_write_str("task:table-full\n");
    return -1;
}

void schedule(void) {
    /* A waiting task is out of the ready pool; with one ready task left
     * (e.g. the demo while the shell blocks on input) it gets the CPU
     * without being preempted. Otherwise we need two ready tasks. */
    if (!current || (current->state != TASK_WAITING && ready_count < 2) || need_switch)
        return;

    task_t *next = 0;
    int my = (int)(current - tasks);
    for (int i = 1; i <= TASK_MAX; i++) {
        task_t *t = &tasks[(my + i) % TASK_MAX];
        if (t->state == TASK_READY) { next = t; break; }
    }
    if (!next) return;

    if (current->state == TASK_RUNNING) current->state = TASK_READY;
    switch_from = &current->esp;
    switch_to = next->esp;
    tss_set_esp0(next->kstack_top);
    next->state = TASK_RUNNING;
    current = next;
    need_switch = 1;
}

void task_wait(void) {
    if (current && current->state == TASK_RUNNING) {
        current->state = TASK_WAITING;
        ready_count--;
    }
}

void task_wake(u32 pid) {
    if (pid < TASK_MAX && tasks[pid].state == TASK_WAITING) {
        tasks[pid].state = TASK_READY;
        ready_count++;
    }
}

void task_exit(void) {
    if (!current) return;

    serial_write_str("task:exit pid=");
    serial_write_int((int)(current - tasks));
    serial_write_str("\n");
    current->state = TASK_DEAD;
    ready_count--;

    if (ready_count <= 0) {
        /* Everything died: safe halt. */
        for (;;) asm volatile("hlt");
    }

    task_t *next = 0;
    for (int i = 0; i < TASK_MAX; i++) {
        if (tasks[i].state == TASK_READY || tasks[i].state == TASK_WAITING) {
            next = &tasks[i];
            if (tasks[i].state == TASK_WAITING) {
                tasks[i].state = TASK_READY;
                ready_count++;   /* picked up by task_exit's own slot */
            }
            break;
        }
    }
    if (!next) { for (;;) asm volatile("hlt"); }

    switch_from = &current->esp;
    switch_to = next->esp;
    tss_set_esp0(next->kstack_top);
    next->state = TASK_RUNNING;
    current = next;
    need_switch = 1;
}

void task_start_shell(u32 entry) {
    int pid = task_create(entry, 0);
    if (pid == -1) return;
    current = &tasks[pid];
    current->state = TASK_RUNNING;
    first_switch(current->esp);
}