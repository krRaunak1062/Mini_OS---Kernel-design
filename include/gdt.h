#ifndef GDT_H
#define GDT_H

#include <stdint.h>

/*
 * gdt_entry_t — one 8-byte GDT descriptor.
 * __attribute__((packed)) tells GCC: do NOT insert any padding bytes.
 * Without this, GCC might add padding and the CPU would read garbage.
 *
 * Implements: REQ-MEM-01
 */
typedef struct __attribute__((packed)) {
    uint16_t limit_low;   /* bits 0–15 of segment limit */
    uint16_t base_low;    /* bits 0–15 of base address  */
    uint8_t  base_mid;    /* bits 16–23 of base address */
    uint8_t  access;      /* Present, DPL, Type flags   */
    uint8_t  granularity; /* Granularity + limit 16–19  */
    uint8_t  base_high;   /* bits 24–31 of base address */
} gdt_entry_t;

/*
 * gdt_ptr_t — what we hand to the LGDT instruction.
 * 'limit' = size of GDT in bytes minus 1.
 * 'base'  = physical address of the GDT array.
 *
 * Implements: REQ-MEM-01
 */
typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint32_t base;
} gdt_ptr_t;

void gdt_init(void);
extern void gdt_flush(uint32_t gdt_ptr_addr);  /* defined in gdt_flush.asm */

#endif /* GDT_H */