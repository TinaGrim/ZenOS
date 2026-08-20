#include "gdt.h"

#define GDT_ENTRIES 6

typedef struct {
    u16 limit_lo;
    u16 base_lo;
    u8  base_mid;
    u8  access;
    u8  gran;
    u8  base_hi;
} __attribute__((packed)) gdt_entry_t;

typedef struct {
    u16 limit;
    u32 base;
} __attribute__((packed)) gdt_reg_t;

static gdt_entry_t gdt[GDT_ENTRIES];
static gdt_reg_t gdt_reg;

/* 80386 TSS (104 bytes). Only esp0/ss0 are used: every ring-3 -> ring-0
 * transition (interrupt or int 0x80) switches onto the esp0 stack. */
typedef struct {
    u32 link, esp0, ss0, esp1, ss1, esp2, ss2;
    u32 cr3, eip, eflags, eax, ecx, edx, ebx;
    u32 esp, ebp, esi, edi, es, cs, ss, ds, es2, fs, gs;
    u32 ldt, iomap;
} tss_t;

static tss_t tss;

static void set_gate(int i, u32 base, u32 limit, u8 access, u8 gran) {
    gdt[i].limit_lo = (u16)(limit & 0xFFFF);
    gdt[i].base_lo  = (u16)(base & 0xFFFF);
    gdt[i].base_mid = (u8)((base >> 16) & 0xFF);
    gdt[i].access   = access;
    gdt[i].gran     = gran;
    gdt[i].base_hi  = (u8)((base >> 24) & 0xFF);
}

void gdt_init(void) {
    set_gate(0, 0, 0, 0, 0);                       /* null */
    set_gate(1, 0, 0xFFFFF, 0x9A, 0xCF);           /* 0x08 kernel code */
    set_gate(2, 0, 0xFFFFF, 0x92, 0xCF);           /* 0x10 kernel data */
    set_gate(3, 0, 0xFFFFF, 0xFA, 0xCF);           /* 0x18 user code (DPL 3) */
    set_gate(4, 0, 0xFFFFF, 0xF2, 0xCF);           /* 0x20 user data (DPL 3) */
    tss.ss0 = GDT_KERNEL_DS;
    tss.esp0 = 0;
    set_gate(5, (u32)&tss, sizeof(tss_t) - 1, 0x89, 0x00); /* 0x28 TSS */

    gdt_reg.limit = sizeof(gdt) - 1;
    gdt_reg.base = (u32)&gdt;

    asm volatile("lgdt %0" : : "m"(gdt_reg));
    asm volatile(
        "movw $0x10, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        "movw %%ax, %%ss\n\t"
        "ljmp $0x08, $reload_cs\n\t"
        "reload_cs:\n\t"
        : : : "ax", "memory");
    asm volatile("ltr %0" : : "r"((u16)GDT_TSS_SEL) : "memory");
}

void tss_set_esp0(u32 esp0) {
    tss.esp0 = esp0;
}