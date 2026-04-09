/*
 * arch/x86/isr_handler.c
 * Common C-level ISR dispatcher.
 *
 * Implements: REQ-MEM-05 (routes ISR 14 to isr_14_handler)
 *             NFR-REL-01  (routes ISR 13 to GPF handler)
 *
 * Owner : Aman Yadav (B24CS1006) — ISR 14 wiring (S2)
 *         Raunak Kumar (B24CS1062) — full handler table (S3)
 *
 * Design:
 *   A fixed table of 32 function pointers, indexed by vector number.
 *   Raunak will extend this in S3 with isr_register_handler() for IRQs.
 *   For S2 we only wire vector 14 (page fault) and vector 13 (GPF).
 */

#include "isr.h"
#include "paging.h"   /* isr_14_handler prototype */
#include "serial.h"
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Handler dispatch table                                              */
/* ------------------------------------------------------------------ */

/* 32 exception vectors (0–31); IRQ handlers added in S3 */
static isr_handler_t isr_handlers[32];

/*
 * isr_register_handler(vector, handler)
 *   Installs a C handler for the given vector.
 *   Called during init for ISR 13 and ISR 14.
 *   Raunak will also call this for IRQ handlers in S3.
 */
void isr_register_handler(uint8_t vector, isr_handler_t handler)
{
    if (vector < 32) {
        isr_handlers[vector] = handler;
    }
}

/* ------------------------------------------------------------------ */
/* GPF handler (ISR 13)                                               */
/* Implements: NFR-REL-01                                              */
/* ------------------------------------------------------------------ */
static void gpf_handler(registers_t *regs)
{
    serial_puts("\n========================================\n");
    serial_puts("  GENERAL PROTECTION FAULT  (ISR 13)\n");
    serial_puts("========================================\n");
    serial_printf("  Error code : 0x%x\n", regs->err_code);
    serial_printf("  EIP        : 0x%x\n", regs->eip);
    serial_printf("  CS         : 0x%x\n", regs->cs);
    serial_printf("  EFLAGS     : 0x%x\n", regs->eflags);
    serial_puts("  Kernel halted.\n");
    serial_puts("========================================\n");

    for (;;) {
        __asm__ __volatile__("hlt");
    }
}

/* ------------------------------------------------------------------ */
/* Wrapper: adapt registers_t* to isr_14_handler(uint32_t err_code)  */
/* ------------------------------------------------------------------ */
static void page_fault_wrapper(registers_t *regs)
{
    isr_14_handler(regs->err_code);
}

/* ------------------------------------------------------------------ */
/* isr_handlers_init                                                   */
/* Called from kernel_main during S2 setup                            */
/* ------------------------------------------------------------------ */
void isr_handlers_init(void)
{
    /* Zero the table (no default handler) */
    for (int i = 0; i < 32; i++) {
        isr_handlers[i] = 0;
    }

    /* Wire ISR 13: General Protection Fault */
    isr_register_handler(13, gpf_handler);

    /* Wire ISR 14: Page Fault */
    isr_register_handler(14, page_fault_wrapper);

    serial_puts("[ISR] Handlers registered: ISR13 (GPF), ISR14 (PF)\n");
}

/* ------------------------------------------------------------------ */
/* isr_common_handler — called from common_isr_stub in isr_stubs.asm  */
/* ------------------------------------------------------------------ */
/*
 * Dispatches to the appropriate handler for the given vector.
 * If no handler is registered, logs the unhandled exception and halts.
 */
void isr_common_handler(registers_t *regs)
{
    uint32_t vec = regs->int_no;

    if (vec < 32 && isr_handlers[vec]) {
        isr_handlers[vec](regs);
    } else {
        serial_printf("[ISR] Unhandled exception vector %d "
                      "(err=0x%x, eip=0x%x)\n",
                      vec, regs->err_code, regs->eip);
        for (;;) {
            __asm__ __volatile__("hlt");
        }
    }
}
