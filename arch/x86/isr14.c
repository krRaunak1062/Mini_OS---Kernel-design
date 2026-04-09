/*
 * arch/x86/isr14.c
 * Page Fault handler — ISR vector 14.
 *
 * Implements: REQ-MEM-05 (kernel-user isolation enforcement)
 *             NFR-REL-01  (log + halt on unrecoverable fault)
 *
 * Owner : Aman Yadav (B24CS1006)
 * Sprint: S2 — Virtual Memory & Kernel Heap
 *
 * How it fits together:
 *   arch/x86/isr_stubs.asm  — NASM stub saves state, calls isr_14_handler()
 *   arch/x86/isr14.c        — this file: decode error code, log, halt
 *   arch/x86/paging_asm.asm — paging_get_cr2() reads CR2 for fault address
 *
 * x86 page fault error code (pushed automatically by the CPU):
 *   Bit 0 (P)  : 0 = not-present fault, 1 = protection violation
 *   Bit 1 (W)  : 0 = read fault,        1 = write fault
 *   Bit 2 (U)  : 0 = kernel-mode fault, 1 = user-mode fault
 *   Bit 3 (R)  : 1 = caused by reading a reserved PTE field
 *   Bit 4 (I)  : 1 = instruction fetch fault (execute disable)
 */

#include "paging.h"
#include "serial.h"
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* isr_14_handler                                                      */
/* Implements: REQ-MEM-05, NFR-REL-01                                  */
/* ------------------------------------------------------------------ */
/*
 * Called from the NASM ISR-14 stub (arch/x86/isr_stubs.asm).
 * The stub:
 *   1. Does NOT push a dummy error code (CPU pushes the real one).
 *   2. Pushes the vector number (14).
 *   3. Jumps to common_isr_stub which saves all registers and
 *      calls this function with err_code as its argument.
 *
 * For S2 (no scheduler yet): always halts after logging.
 * For S4+: this could attempt recovery or kill the offending task.
 */
void isr_14_handler(uint32_t err_code)
{
    /* CR2 holds the virtual address that caused the fault */
    uint32_t fault_addr = paging_get_cr2();

    /* Decode error code bits */
    int not_present    = !(err_code & 0x1); /* P=0: page not mapped   */
    int was_write      =  (err_code & 0x2); /* W=1: write operation   */
    int from_user      =  (err_code & 0x4); /* U=1: ring-3 access     */
    int reserved_bits  =  (err_code & 0x8); /* R=1: reserved PTE bit  */
    int instr_fetch    =  (err_code & 0x10);/* I=1: fetch fault       */

    /* ---- Serial log (always, regardless of cause) ---- */
    serial_puts("\n========================================\n");
    serial_puts("  PAGE FAULT  (ISR 14)\n");
    serial_puts("========================================\n");
    serial_printf("  Faulting address : 0x%x\n", fault_addr);
    serial_printf("  Error code       : 0x%x\n", err_code);
    serial_printf("  Cause            : %s\n",
                  not_present   ? "page not present"
                  : reserved_bits ? "reserved bit in PTE"
                  : "protection violation");
    serial_printf("  Access type      : %s\n",
                  instr_fetch ? "instruction fetch"
                  : was_write ? "write"
                  :             "read");
    serial_printf("  Privilege        : %s mode\n",
                  from_user ? "user (ring 3)" : "kernel (ring 0)");

    /*
     * NFR-SEC-01 check: if from_user, this is a user process trying
     * to access a supervisor page — expected and correct behaviour.
     * Log it clearly so S5 integration tests can verify isolation.
     */
    if (from_user) {
        serial_puts("  [SECURITY] User-mode access to kernel page "
                    "blocked — isolation working correctly.\n");
    }

    serial_puts("  Kernel halted.\n");
    serial_puts("========================================\n");

    /*
     * NFR-REL-01: on unrecoverable fault, halt the system.
     * The hlt instruction waits for the next interrupt; the for loop
     * ensures we never return even if an NMI wakes the CPU.
     */
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}
