#ifndef ISR_H
#define ISR_H

/*
 * include/isr.hm
 * Implements: REQ-TASK-01, REQ-TASK-02, REQ-MEM-05
 *
 * Single header for all ISR/IRQ types and declarations.
 * Covers both S2 (handler table, isr_register_handler) and
 * S3 (full stub externs, isr_handler, irq_handler, isr_install).
 *
 * S2 files (isr_handler.c, isr14.c) are kept exactly as-is.
 * S3 adds isr.c on top — no S2 file is removed or modified.
 *
 * Authors:
 *   Aman Yadav   (B24CS1006) — registers_t, isr_handler_t,
 *                              isr_register_handler (S2)
 *   Raunak Kumar (B24CS1062) — isr_handler, irq_handler,
 *                              isr_install, stub externs (S3)
 */

#include <stdint.h>

/* ------------------------------------------------------------------
 * registers_t — full CPU state pushed by the NASM common stub.
 *
 * Layout must match isr_stubs.asm EXACTLY:
 *
 *   [esp+ 0] ds          pushed manually after pusha
 *   [esp+ 4] edi    \
 *   [esp+ 8] esi     |
 *   [esp+12] ebp     |   pusha  (edi ends up at top)
 *   [esp+16] esp     |
 *   [esp+20] ebx     |
 *   [esp+24] edx     |
 *   [esp+28] ecx     |
 *   [esp+32] eax    /
 *   [esp+36] int_no      vector number, pushed by stub
 *   [esp+40] err_code    CPU error code or dummy 0
 *   [esp+44] eip    \
 *   [esp+48] cs      |   pushed automatically by CPU
 *   [esp+52] eflags  |
 *   [esp+56] useresp |   only on privilege-level change
 *   [esp+60] ss     /
 *
 * NOTE (S2 fix): the original S2 registers_t was missing useresp
 * and ss.  They are required because IRET always pops all five
 * CPU-pushed fields.  Without them the struct is 8 bytes short and
 * eip/cs/eflags are read from the wrong offsets.
 * Implements: REQ-TASK-01, REQ-TASK-02
 * ------------------------------------------------------------------ */
/*
 * Ring-0 kernel interrupt stack frame.
 * The CPU does NOT push ss/useresp for ring-0 → ring-0 interrupts.
 *
 * Save sequence in common_isr_stub / common_irq_stub:
 *   pushad  → EDI at lowest addr (offset 4 in struct, after ds)
 *   push gs
 *   push fs
 *   push es
 *   push ds → ds at LOWEST addr = offset 0 = registers_t.ds  ✓
 *
 * Full layout from esp upward after all saves:
 *   [+0]  ds         manually pushed last → struct offset 0
 *   [+4]  es
 *   [+8]  fs
 *   [+12] gs
 * BUG FIX (Bug 2): GP register order corrected to match NASM PUSHAD.
 *
 * NASM PUSHAD pushes: EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI
 *   EAX is pushed FIRST → ends up at the HIGHEST address in the block.
 *   EDI is pushed LAST  → ends up at the LOWEST address (lowest offset).
 *
 * So the actual stack layout from esp upward (lowest offset = lowest addr):
 *   [+16] edi  \   EDI pushed last by PUSHAD → lowest offset in block
 *   [+20] esi   |
 *   [+24] ebp   |  PUSHAD block — field order matches push order
 *   [+28] esp   |  (esp_dummy = ESP value captured by PUSHAD)
 *   [+32] ebx   |
 *   [+36] edx   |
 *   [+40] ecx   |
 *   [+44] eax  /   EAX pushed first by PUSHAD → highest offset in block
 *   [+48] int_no    pushed by ISR/IRQ macro
 *   [+52] err_code  pushed by ISR/IRQ macro (real or dummy 0)
 *   [+56] eip   \
 *   [+60] cs     |  CPU auto-pushed (ring-0, no privilege change)
 *   [+64] eflags/
 *
 * IMPORTANT: int_no, err_code, eip, cs, eflags are at the SAME offsets
 * regardless of GP order (they sit above the entire pushad block).
 * Current code only reads those fields so no crash occurs with the old
 * wrong order — but any future handler reading regs->eax for a syscall
 * number would silently receive regs->edi instead. Fixed here.
 */
typedef struct {
    uint32_t ds;                                    /* manually pushed last */
    uint32_t es, fs, gs;                            /* segment regs         */
    /* PUSHAD: EDI pushed last = lowest offset; EAX pushed first = highest */
    uint32_t edi, esi, ebp, esp_dummy,              /* pushad block —       */
             ebx, edx, ecx, eax;                    /* correct NASM order   */
    uint32_t int_no, err_code;                      /* stub-pushed          */
    uint32_t eip, cs, eflags;                       /* CPU auto-pushed      */
} __attribute__((packed)) registers_t;

/* ------------------------------------------------------------------
 * isr_handler_t — C-level handler function pointer type.
 * Used by isr_register_handler() (S2).
 * Implements: REQ-TASK-02
 * ------------------------------------------------------------------ */
typedef void (*isr_handler_t)(registers_t *);

/* ------------------------------------------------------------------
 * S2 interface — provided by arch/x86/isr_handler.c
 * ------------------------------------------------------------------ */

/**
 * isr_register_handler() — install a C handler for a specific vector.
 * Called from isr_handlers_init() for ISR 13 and ISR 14.
 * Implements: REQ-TASK-02
 */
void isr_register_handler(uint8_t vector, isr_handler_t handler);

/**
 * isr_handlers_init() — wire ISR 13 (GPF) and ISR 14 (PF) handlers.
 * Called from kernel_main during S2 setup (before idt_load_minimal).
 * NOT called in S3 kernel_main — S3 calls isr_install() instead.
 * Kept declared here so isr_handler.c still compiles cleanly.
 * Implements: REQ-MEM-05, NFR-REL-01
 */
void isr_handlers_init(void);

/**
 * isr_common_handler() — S2 dispatcher, defined in isr_handler.c.
 * Called internally by S3's isr_handler() to forward exceptions
 * through the S2 handler table.
 * Implements: REQ-TASK-02
 */
void isr_common_handler(registers_t *regs);

/* ------------------------------------------------------------------
 * S3 interface — provided by arch/x86/isr.c
 * ------------------------------------------------------------------ */

/**
 * isr_handler() — NASM stub entry point for CPU exceptions 0–31.
 * Forwards to isr_common_handler() (S2 table).
 * Implements: REQ-TASK-02
 */
void isr_handler(registers_t *regs);

/**
 * irq_handler() — NASM stub entry point for hardware IRQs 0–15.
 * Pure S3 new work — S2 had no IRQ handling.
 * Implements: REQ-TASK-02
 */
void irq_handler(registers_t *regs);

/**
 * isr_install() — log that all handlers are active.
 * Called from kernel_main after pic_remap() + pit_init(), before STI.
 * Implements: REQ-TASK-01, REQ-TASK-02
 */
void isr_install(void);

/* ------------------------------------------------------------------
 * NASM exception stubs — vectors 0–31.
 * Defined in arch/x86/isr_stubs.asm.
 * Implements: REQ-TASK-01
 * ------------------------------------------------------------------ */
extern void isr_0(void);  extern void isr_1(void);
extern void isr_2(void);  extern void isr_3(void);
extern void isr_4(void);  extern void isr_5(void);
extern void isr_6(void);  extern void isr_7(void);
extern void isr_8(void);  extern void isr_9(void);
extern void isr_10(void); extern void isr_11(void);
extern void isr_12(void); extern void isr_13(void);
extern void isr_14(void); extern void isr_15(void);
extern void isr_16(void); extern void isr_17(void);
extern void isr_18(void); extern void isr_19(void);
extern void isr_20(void); extern void isr_21(void);
extern void isr_22(void); extern void isr_23(void);
extern void isr_24(void); extern void isr_25(void);
extern void isr_26(void); extern void isr_27(void);
extern void isr_28(void); extern void isr_29(void);
extern void isr_30(void); extern void isr_31(void);

/* ------------------------------------------------------------------
 * NASM IRQ stubs — IRQ 0–15 → IDT vectors 32–47.
 * Defined in arch/x86/isr_stubs.asm.
 * Implements: REQ-TASK-02
 * ------------------------------------------------------------------ */
extern void irq_0(void);  extern void irq_1(void);
extern void irq_2(void);  extern void irq_3(void);
extern void irq_4(void);  extern void irq_5(void);
extern void irq_6(void);  extern void irq_7(void);
extern void irq_8(void);  extern void irq_9(void);
extern void irq_10(void); extern void irq_11(void);
extern void irq_12(void); extern void irq_13(void);
extern void irq_14(void); extern void irq_15(void);

#endif /* ISR_H */