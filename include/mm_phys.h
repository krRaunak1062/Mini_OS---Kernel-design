#ifndef MM_PHYS_H
#define MM_PHYS_H

#include <stdint.h>
#include "multiboot.h"

/*
 * mm_init     — parse Multiboot map, build free-frame bitmap.
 *               Call once, early in kernel_main, after gdt_init().
 * mm_alloc_frame — return physical address of a free 4KB frame (or 0).
 * mm_free_frame  — release a frame back to the pool.
 *
 * Implements: REQ-MEM-03
 */
void     mm_init(multiboot_info_t *mbi);
uint32_t mm_alloc_frame(void);
void     mm_free_frame(uint32_t addr);

#endif /* MM_PHYS_H */