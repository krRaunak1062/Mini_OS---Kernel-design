/**
 * arch/x86/idt.c
 * Implements: REQ-TASK-01
 *
 * Builds the 256-entry Interrupt Descriptor Table and installs it
 * via idt_flush() (LIDT trampoline in idt_flush.asm).
 */

#include "idt.h"
#include "isr.h"
#include <stdint.h>

/* The IDT: 256 gates × 8 bytes each. */
static idt_entry_t idt[256];
static idt_ptr_t   idt_ptr;

/* ---------------------------------------------------------------
 * idt_set_gate — fill one IDT descriptor.
 *
 * @num   : vector number (0–255)
 * @base  : handler address
 * @sel   : code segment selector (always 0x08 — kernel CS)
 * @flags : e.g. 0x8E = Present | DPL=0 | 32-bit interrupt gate
 * Implements: REQ-TASK-01
 * --------------------------------------------------------------- */
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags)
{
    idt[num].base_low  = (uint16_t)(base & 0xFFFF);
    idt[num].base_high = (uint16_t)((base >> 16) & 0xFFFF);
    idt[num].sel       = sel;
    idt[num].always0   = 0;
    idt[num].flags     = flags;
}

/* ---------------------------------------------------------------
 * idt_init — zero the table, wire all 256 gates, then LIDT.
 * Implements: REQ-TASK-01
 * --------------------------------------------------------------- */
void idt_init(void)
{
    idt_ptr.limit = (sizeof(idt_entry_t) * 256) - 1;
    idt_ptr.base  = (uint32_t)&idt;

    /*
     * FIX (Bug 3): Default-fill all 256 entries as NOT PRESENT.
     *
     * Original code used flags=0x8E (Present=1) with base=0, meaning
     * any stray interrupt on an unhandled vector (48–255) would jump
     * to address 0x00000000, executing garbage and causing a triple fault.
     *
     * 0x0E = 32-bit interrupt gate, DPL=0, Present=0.
     * A not-present gate causes a #NP (Segment Not Present) exception
     * on vectors 48–255 instead of jumping to address 0.  This is
     * a controlled, debuggable failure mode rather than a silent crash.
     */
    for (int i = 0; i < 256; i++) {
        idt_set_gate(i, 0, 0x08, 0x0E);   /* NOT present — safe default */
    }

    /* CPU exception stubs 0–31 (overwrite with Present=1, 0x8E) */
    idt_set_gate(0,  (uint32_t)isr_0,  0x08, 0x8E);
    idt_set_gate(1,  (uint32_t)isr_1,  0x08, 0x8E);
    idt_set_gate(2,  (uint32_t)isr_2,  0x08, 0x8E);
    idt_set_gate(3,  (uint32_t)isr_3,  0x08, 0x8E);
    idt_set_gate(4,  (uint32_t)isr_4,  0x08, 0x8E);
    idt_set_gate(5,  (uint32_t)isr_5,  0x08, 0x8E);
    idt_set_gate(6,  (uint32_t)isr_6,  0x08, 0x8E);
    idt_set_gate(7,  (uint32_t)isr_7,  0x08, 0x8E);
    idt_set_gate(8,  (uint32_t)isr_8,  0x08, 0x8E);
    idt_set_gate(9,  (uint32_t)isr_9,  0x08, 0x8E);
    idt_set_gate(10, (uint32_t)isr_10, 0x08, 0x8E);
    idt_set_gate(11, (uint32_t)isr_11, 0x08, 0x8E);
    idt_set_gate(12, (uint32_t)isr_12, 0x08, 0x8E);
    idt_set_gate(13, (uint32_t)isr_13, 0x08, 0x8E);  /* GPF */
    idt_set_gate(14, (uint32_t)isr_14, 0x08, 0x8E);  /* PF  */
    idt_set_gate(15, (uint32_t)isr_15, 0x08, 0x8E);
    idt_set_gate(16, (uint32_t)isr_16, 0x08, 0x8E);
    idt_set_gate(17, (uint32_t)isr_17, 0x08, 0x8E);
    idt_set_gate(18, (uint32_t)isr_18, 0x08, 0x8E);
    idt_set_gate(19, (uint32_t)isr_19, 0x08, 0x8E);
    idt_set_gate(20, (uint32_t)isr_20, 0x08, 0x8E);
    idt_set_gate(21, (uint32_t)isr_21, 0x08, 0x8E);
    idt_set_gate(22, (uint32_t)isr_22, 0x08, 0x8E);
    idt_set_gate(23, (uint32_t)isr_23, 0x08, 0x8E);
    idt_set_gate(24, (uint32_t)isr_24, 0x08, 0x8E);
    idt_set_gate(25, (uint32_t)isr_25, 0x08, 0x8E);
    idt_set_gate(26, (uint32_t)isr_26, 0x08, 0x8E);
    idt_set_gate(27, (uint32_t)isr_27, 0x08, 0x8E);
    idt_set_gate(28, (uint32_t)isr_28, 0x08, 0x8E);
    idt_set_gate(29, (uint32_t)isr_29, 0x08, 0x8E);
    idt_set_gate(30, (uint32_t)isr_30, 0x08, 0x8E);
    idt_set_gate(31, (uint32_t)isr_31, 0x08, 0x8E);

    /* IRQ stubs 0–15 → IDT vectors 32–47 (after PIC remap) */
    idt_set_gate(32, (uint32_t)irq_0,  0x08, 0x8E);
    idt_set_gate(33, (uint32_t)irq_1,  0x08, 0x8E);
    idt_set_gate(34, (uint32_t)irq_2,  0x08, 0x8E);
    idt_set_gate(35, (uint32_t)irq_3,  0x08, 0x8E);
    idt_set_gate(36, (uint32_t)irq_4,  0x08, 0x8E);
    idt_set_gate(37, (uint32_t)irq_5,  0x08, 0x8E);
    idt_set_gate(38, (uint32_t)irq_6,  0x08, 0x8E);
    idt_set_gate(39, (uint32_t)irq_7,  0x08, 0x8E);
    idt_set_gate(40, (uint32_t)irq_8,  0x08, 0x8E);
    idt_set_gate(41, (uint32_t)irq_9,  0x08, 0x8E);
    idt_set_gate(42, (uint32_t)irq_10, 0x08, 0x8E);
    idt_set_gate(43, (uint32_t)irq_11, 0x08, 0x8E);
    idt_set_gate(44, (uint32_t)irq_12, 0x08, 0x8E);
    idt_set_gate(45, (uint32_t)irq_13, 0x08, 0x8E);
    idt_set_gate(46, (uint32_t)irq_14, 0x08, 0x8E);
    idt_set_gate(47, (uint32_t)irq_15, 0x08, 0x8E);

    idt_flush((uint32_t)&idt_ptr);
}