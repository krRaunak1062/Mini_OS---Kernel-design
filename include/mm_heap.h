#ifndef MM_HEAP_H
#define MM_HEAP_H

/*
 * include/mm_heap.h
 * Kernel heap allocator public API (S2)
 * Implements: REQ-MEM-04
 *
 * Owner : Aman Yadav (B24CS1006)
 * Sprint: S2 — Virtual Memory & Kernel Heap
 *
 * Call mm_heap_init() AFTER paging_enable() in kernel_main.
 */

#include <stdint.h>

void  mm_heap_init(void);
void *kmalloc(uint32_t size);
void  kfree(void *ptr);
void  mm_heap_dump(void);   /* diagnostic — prints heap state to serial */

#endif /* MM_HEAP_H */