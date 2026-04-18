/**
 * arch/x86/isr.c
 * Implements: REQ-TASK-01, REQ-TASK-02
 *
 * Sprint 3 — interrupt dispatcher layer.
 *
 * irq_handler() is called by irq_common_stub (NASM) for hardware IRQs.
 *
 * For IRQ0 (PIT timer):
 *   pit_irq0_handler() is called.  That function:
 *     1. Increments pit_tick_count.
 *     2. Sends EOI to master PIC (pic_send_eoi(0)).
 *     3. Calls sched_switch().
 *
 *   sched_switch() calls context_switch(old, new).
 *   For a NEW task, context_switch()'s iret jumps directly to the
 *   task function.  The new task runs until the next IRQ0.
 *   For an EXISTING task, context_switch()'s iret lands at .resume
 *   in context_switch.asm, which falls through, returning here to
 *   irq_handler() — which returns to irq_common_stub — which irets
 *   back into the previous task's interrupted execution point.
 *
 * DO NOT call sched_switch() directly here — it is called inside
 * pit_irq0_handler().  Calling it twice would switch tasks twice per
 * tick and skip tasks.
 *
 * Authors:
 *   Aman Yadav   (B24CS1006) — S2 handler table (isr_handler.c)
 *   Raunak Kumar (B24CS1062) — S3 NASM bridge + IRQ dispatch (this file)
 */

#include "isr.h"
#include "pic.h"
#include "pit.h"
#include <stdint.h>

extern void serial_puts(const char *s);

static void serial_puthex(uint32_t val)
{
    static const char hex[] = "0123456789ABCDEF";
    char buf[11];
    buf[0]  = '0';
    buf[1]  = 'x';
    buf[10] = '\0';
    for (int i = 9; i >= 2; i--) {
        buf[i] = hex[val & 0xF];
        val >>= 4;
    }
    serial_puts(buf);
}

extern void isr_common_handler(registers_t *regs);

/* ------------------------------------------------------------------
 * isr_handler — entry for CPU exceptions (vectors 0–31).
 * Called by isr_common_stub in isr_stubs.asm.
 * Implements: REQ-TASK-02
 * ------------------------------------------------------------------ */
void isr_handler(registers_t *regs)
{
    isr_common_handler(regs);
}

/* ------------------------------------------------------------------
 * irq_handler — entry for hardware IRQs (vectors 32–47).
 * Called by irq_common_stub in isr_stubs.asm.
 *
 * IMPORTANT: Do NOT call sched_switch() here.
 * sched_switch() is called inside pit_irq0_handler() which is called
 * from case 0 below.  Adding another sched_switch() call here would
 * cause a double-switch per tick, skipping tasks.
 *
 * Implements: REQ-TASK-02
 * ------------------------------------------------------------------ */
void irq_handler(registers_t *regs)
{
    uint8_t irq = (uint8_t)regs->int_no;

    switch (irq) {

        case 0:
            /*
             * IRQ0 — PIT timer (100 Hz, 10ms quantum).
             * pit_irq0_handler() does all three things in order:
             *   1. pit_tick_count++
             *   2. pic_send_eoi(0)   — EOI BEFORE context switch
             *   3. sched_switch()    — may not return for new tasks
             */
            pit_irq0_handler();
            break;

        default:
            /* Unhandled IRQ — send EOI or the PIC line stays masked. */
            serial_puts("[IRQ] Unhandled IRQ ");
            serial_puthex(irq);
            serial_puts("\n");
            pic_send_eoi(irq);
            break;
    }
}

/* ------------------------------------------------------------------
 * isr_install — called from kernel_main after pic_remap() + pit_init().
 * Implements: REQ-TASK-01, REQ-TASK-02
 * ------------------------------------------------------------------ */
void isr_install(void)
{
    isr_handlers_init();
    serial_puts("[ISR] S3 handlers installed\n");
}