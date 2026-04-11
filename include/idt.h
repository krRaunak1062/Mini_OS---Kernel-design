#ifndef IDT_H
#define IDT_H

#include <stdint.h>
#include "isr.h"   /* for registers_t */
/**
 * idt_entry_t — one 8-byte descriptor in the IDT.
 * Implements: REQ-TASK-01
 */
typedef struct {
    uint16_t base_low;   /* bits 0–15  of handler address  */
    uint16_t sel;        /* code segment selector (0x08)   */
    uint8_t  always0;    /* reserved, must be zero         */
    uint8_t  flags;      /* P | DPL | 0 | gate type        */
    uint16_t base_high;  /* bits 16–31 of handler address  */
} __attribute__((packed)) idt_entry_t;

/**
 * idt_ptr_t — operand for LIDT instruction.
 * Implements: REQ-TASK-01
 */
typedef struct {
    uint16_t limit;  /* size of IDT in bytes minus 1 */
    uint32_t base;   /* linear address of IDT        */
} __attribute__((packed)) idt_ptr_t;

/**
 * registers_t — full CPU state pushed by ISR stubs before
 * calling the C handler.  Layout must match isr_stubs.asm exactly.
 * Implements: REQ-TASK-01, REQ-TASK-02
 */
// typedef struct {
//     uint32_t ds;                                 /* pushed manually      */
//     uint32_t edi, esi, ebp, esp_dummy,           /* PUSHA order          */
//              ebx, edx, ecx, eax;
//     uint32_t int_no, err_code;                   /* pushed by stub       */
//     uint32_t eip, cs, eflags, useresp, ss;       /* pushed by CPU        */
// } __attribute__((packed)) registers_t;

/* Initialise and load the 256-entry IDT.
 * Implements: REQ-TASK-01 */
void idt_init(void);

/* NASM trampoline — executes LIDT and returns.
 * Implements: REQ-TASK-01 */
extern void idt_flush(uint32_t idt_ptr_addr);

/* Install a single gate into the IDT.
 * Implements: REQ-TASK-01 */
void idt_set_gate(uint8_t num, uint32_t base, uint16_t sel, uint8_t flags);

#endif /* IDT_H */
