#ifndef ISR_H
#define ISR_H

/*
 * include/isr.h
 * IDT gate descriptor and ISR handler type definitions.
 * Used by S2 (ISR 14) and S3 (full 256-entry IDT).
 */

#include <stdint.h>

/* Packed register state pushed by the common ISR stub */
typedef struct {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp_dummy, ebx, edx, ecx, eax; /* PUSHAD */
    uint32_t int_no;    /* vector number pushed by stub */
    uint32_t err_code;  /* error code (or dummy 0) pushed by stub */
    /* CPU auto-pushed on entry to gate: */
    uint32_t eip, cs, eflags;
} __attribute__((packed)) registers_t;

/* Function pointer type for C-level ISR handlers */
typedef void (*isr_handler_t)(registers_t *);

/* Install a handler for a given vector (used by S3 IDT init) */
void isr_register_handler(uint8_t vector, isr_handler_t handler);

#endif /* ISR_H */
