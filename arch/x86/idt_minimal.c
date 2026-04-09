/*
 * arch/x86/idt_minimal.c
 * Minimal IDT setup for Sprint 2.
 *
 * Implements: REQ-MEM-05 (installs ISR 14 so page faults are caught)
 *             NFR-REL-01 (installs ISR 13 so GPFs are caught)
 *
 * Owner : Aman Yadav (B24CS1006) — S2 minimal version
 *         Raunak Kumar (B24CS1062) — will replace with full 256-entry IDT in S3
 *
 * This file will be REPLACED entirely by Raunak's arch/x86/idt.c in S3.
 * It exists only to let S2 run with page fault detection before S3.
 *
 * IDT gate format (32-bit interrupt gate):
 *   Bits [15: 0] : handler address low 16 bits
 *   Bits [31:16] : segment selector (0x08 = kernel code segment)
 *   Bits [39:32] : 0x00 (reserved)
 *   Bits [47:40] : type+attributes = 0x8E
 *                    1000 = present
 *                    00   = DPL 0 (kernel)
 *                    0    = storage segment = 0
 *                    1110 = 32-bit interrupt gate
 *   Bits [63:48] : handler address high 16 bits
 */

#include <stdint.h>

/* ------------------------------------------------------------------ */
/* IDT gate descriptor (64-bit, packed)                               */
/* ------------------------------------------------------------------ */
typedef struct {
    uint16_t offset_low;    /* handler addr bits [15:0]  */
    uint16_t selector;      /* kernel code segment = 0x08 */
    uint8_t  zero;          /* always 0                   */
    uint8_t  type_attr;     /* 0x8E = present, DPL0, 32-bit interrupt gate */
    uint16_t offset_high;   /* handler addr bits [31:16]  */
} __attribute__((packed)) idt_gate_t;

/* IDTR: 6-byte descriptor loaded with LIDT */
typedef struct {
    uint16_t limit;         /* size of IDT in bytes - 1   */
    uint32_t base;          /* linear address of IDT       */
} __attribute__((packed)) idtr_t;

/* ------------------------------------------------------------------ */
/* IDT storage                                                         */
/* ------------------------------------------------------------------ */

/* 256 gates for a full IDT — Raunak fills the rest in S3 */
static idt_gate_t idt_entries[256];
static idtr_t     idtr;

/* ------------------------------------------------------------------ */
/* External ISR stub symbols from arch/x86/isr_stubs.asm             */
/* ------------------------------------------------------------------ */
extern void isr0(void);
extern void isr13(void);
extern void isr14(void);

/* NASM lidt wrapper from arch/x86/idt_asm.asm */
extern void idt_flush(uint32_t idtr_addr);

/* ------------------------------------------------------------------ */
/* idt_set_gate (internal helper)                                     */
/* ------------------------------------------------------------------ */
static void idt_set_gate(uint8_t vector, uint32_t handler)
{
    idt_entries[vector].offset_low  = handler & 0xFFFF;
    idt_entries[vector].selector    = 0x08;   /* kernel code segment */
    idt_entries[vector].zero        = 0;
    idt_entries[vector].type_attr   = 0x8E;   /* present, DPL0, int gate */
    idt_entries[vector].offset_high = (handler >> 16) & 0xFFFF;
}

/* ------------------------------------------------------------------ */
/* idt_load_minimal                                                   */
/* Implements: REQ-MEM-05                                              */
/* ------------------------------------------------------------------ */
/*
 * Sets up a minimal IDT with entries for:
 *   ISR 0  — divide-by-zero (useful for basic testing)
 *   ISR 13 — general protection fault (NFR-REL-01)
 *   ISR 14 — page fault (REQ-MEM-05) ← the critical one for S2
 *
 * All other entries are zeroed (not-present) — if any other exception
 * fires, the CPU will double fault (ISR 8) which QEMU will report.
 *
 * Raunak replaces this entire function in S3 with a full 256-gate IDT.
 */
void idt_load_minimal(void)
{
    /* Zero all 256 entries */
    for (int i = 0; i < 256; i++) {
        idt_entries[i].offset_low  = 0;
        idt_entries[i].selector    = 0;
        idt_entries[i].zero        = 0;
        idt_entries[i].type_attr   = 0;
        idt_entries[i].offset_high = 0;
    }

    /* Install the three handlers needed for S2 */
    idt_set_gate(0,  (uint32_t)isr0);
    idt_set_gate(13, (uint32_t)isr13);
    idt_set_gate(14, (uint32_t)isr14);

    /* Fill IDTR and call LIDT */
    idtr.limit = sizeof(idt_entries) - 1;
    idtr.base  = (uint32_t)&idt_entries;

    idt_flush((uint32_t)&idtr);
}
