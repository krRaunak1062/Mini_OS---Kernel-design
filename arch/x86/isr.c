/**
 * arch/x86/isr.c
 * Implements: REQ-TASK-01, REQ-TASK-02
 *
 * Sprint 3 — interrupt dispatcher layer.
 *
 * DESIGN: This file does NOT replace any S2 file.
 *   S2 files kept exactly as-is:
 *     arch/x86/isr_handler.c  — handler table, isr_common_handler(),
 *                               isr_handlers_init(), GPF + PF wiring
 *     arch/x86/isr14.c        — full page fault decoder
 *
 *   This file adds:
 *     isr_handler()   — called by isr_common_stub (NASM) for exceptions
 *     irq_handler()   — called by irq_common_stub (NASM) for IRQs
 *     isr_install()   — called from kernel_main after pic_remap()
 *
 *   isr_handler() simply forwards to S2's isr_common_handler() so
 *   all S2-registered handlers (ISR 13, ISR 14) continue to fire
 *   exactly as before.
 *
 * Authors:
 *   Aman Yadav   (B24CS1006) — S2 handler table (isr_handler.c)
 *   Raunak Kumar (B24CS1062) — S3 NASM bridge + IRQ dispatch (this file)
 */

#include "isr.h"
#include "pic.h"
#include "pit.h"
#include <stdint.h>

/* S0 interface */
extern void vga_puts(const char *s);
extern void serial_puts(const char *s);

/*
 * serial_puthex — local helper so this file has no dependency on
 * whether S0 exports serial_puthex() or not.
 */
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

/*
 * Forward declaration — isr_common_handler() is defined in S2's
 * isr_handler.c and dispatches through the S2 handler table.
 * We call it from isr_handler() below so ISR 13 and ISR 14 keep
 * working exactly as Aman wired them in S2.
 */
extern void isr_common_handler(registers_t *regs);

/* ------------------------------------------------------------------
 * isr_handler — entry point for CPU exceptions (vectors 0–31).
 *
 * Called by isr_common_stub in isr_stubs.asm.
 * Forwards to S2's isr_common_handler() which already handles
 * ISR 13 (GPF) and ISR 14 (page fault) via its dispatch table.
 * Any vector with no registered handler is caught by the default
 * path below.
 * Implements: REQ-TASK-02
 * ------------------------------------------------------------------ */
void isr_handler(registers_t *regs)
{
    /*
     * Delegate to S2's handler table first.
     * This fires isr_14_handler() for #PF and gpf_handler() for #GP,
     * exactly as set up in isr_handlers_init() during S2 init.
     */
    isr_common_handler(regs);

    /*
     * isr_common_handler() halts for every vector it handles (13, 14)
     * and also halts for unhandled vectors — so we only reach here
     * for vectors it explicitly returns from (currently none in S2).
     * Leave this path open for S4 where some exceptions may recover.
     */
}

/* ------------------------------------------------------------------
 * irq_handler — entry point for hardware IRQs (vectors 32–47).
 *
 * Called by irq_common_stub in isr_stubs.asm.
 * S2 had no IRQ handling at all — this is pure S3 new work.
 * Implements: REQ-TASK-02
 * ------------------------------------------------------------------ */
void irq_handler(registers_t *regs)
{
    /*
     * int_no holds the raw IRQ number (0-15) pushed by the IRQ_STUB macro.
     * Do NOT subtract 32 — the stub pushes the IRQ number directly, not
     * the IDT vector number. Subtracting 32 from 0 would underflow to 0xF0.
     */
    uint8_t irq = (uint8_t)regs->int_no;

    switch (irq) {

        case 0:
            /*
             * IRQ0 — PIT timer tick (100 Hz).
             * pit_irq0_handler() increments pit_tick_count and
             * sends EOI to master PIC.  sched_switch() will be
             * called from here in S4.
             */
            pit_irq0_handler();
            break;

        default:
            /*
             * Unhandled IRQ — must send EOI or the PIC will never
             * re-fire this line again (IRQ line stays masked).
             */
            serial_puts("[IRQ] Unhandled IRQ ");
            serial_puthex(irq);
            serial_puts("\n");
            pic_send_eoi(irq);
            break;
    }
}

/* ------------------------------------------------------------------
 * isr_install — called once from kernel_main after pic_remap()
 *               and pit_init(), before STI.
 * Implements: REQ-TASK-01, REQ-TASK-02
 * ------------------------------------------------------------------ */
void isr_install(void)
{
    /*
     * Bug Fix 4: isr_handlers_init() registers ISR 13 (GPF) and
     * ISR 14 (#PF) into isr_handler.c's dispatch table.
     * kernel_main no longer calls it directly (S2 did; S3 removed
     * the call when it dropped idt_load_minimal). Without this call
     * the table is all-NULL and every exception hits the unhandled halt.
     */
    isr_handlers_init();

    serial_puts("[ISR] S3 handlers installed "
                "(exceptions -> S2 table, IRQs 0-15 -> S3 dispatch)\n");
}