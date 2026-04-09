/*
 * mm/heap.c
 * Kernel heap allocator — free-list implementation.
 *
 * Implements: REQ-MEM-04 (kmalloc / kfree)
 *
 * Owner : Aman Yadav (B24CS1006)
 * Sprint: S2 — Virtual Memory & Kernel Heap
 *
 * Memory layout:
 *   HEAP_START = 0xC0400000  (first address after 4MB kernel mapping)
 *   HEAP_SIZE  = 4MB = 0x400000 bytes
 *   HEAP_END   = 0xC0800000
 *
 *   This region is already mapped by paging_map_kernel() which maps
 *   virt 0xC0000000–0xC03FFFFF. To cover the heap at 0xC0400000 you
 *   must extend the kernel map OR map the heap explicitly. See
 *   kernel_main.c where paging_map_heap() is called after
 *   paging_map_kernel().
 *
 * Allocator design — implicit free list (first-fit):
 *   Each allocation is prefixed by a block_header_t struct.
 *   On mm_heap_init() the entire heap is one large free block.
 *   kmalloc() walks the list, splits if the block is large enough.
 *   kfree()   marks a block free and coalesces adjacent free blocks.
 *
 * Thread safety:
 *   S2 is single-threaded. Raunak adds locking in S4 when concurrent
 *   tasks call kmalloc/kfree simultaneously (S5 stress test).
 */

#include "mm_heap.h"
#include "mm_phys.h"
#include "serial.h"
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Heap constants                                                       */
/* ------------------------------------------------------------------ */

#define HEAP_START  0xC0400000U
#define HEAP_SIZE   (4U * 1024U * 1024U)   /* 4MB */
#define HEAP_END    (HEAP_START + HEAP_SIZE)

/*
 * Minimum allocation size (prevents tiny slivers that waste header space).
 * Any kmalloc(n) where n < MIN_ALLOC is rounded up to MIN_ALLOC.
 */
#define MIN_ALLOC   16U

/* ------------------------------------------------------------------ */
/* Block header                                                         */
/* ------------------------------------------------------------------ */

/*
 * Sits immediately before every allocated or free block.
 * Total allocation = sizeof(block_header_t) + size bytes.
 */
typedef struct block_header {
    uint32_t            size;   /* usable bytes after this header     */
    uint8_t             used;   /* 1 = allocated, 0 = free            */
    uint8_t             _pad[3];
    struct block_header *next;  /* next block in list (NULL = last)   */
} block_header_t;

/* Pointer to the first block (start of the heap free list) */
static block_header_t *heap_head = 0;

/* ------------------------------------------------------------------ */
/* mm_heap_init                                                         */
/* Implements: REQ-MEM-04                                              */
/* ------------------------------------------------------------------ */
/*
 * Must be called after paging_enable() and after the heap virtual
 * address range is mapped.
 * Sets up a single large free block covering the entire heap.
 */
void mm_heap_init(void)
{
    heap_head = (block_header_t *)HEAP_START;
    heap_head->size = HEAP_SIZE - sizeof(block_header_t);
    heap_head->used = 0;
    heap_head->next = 0;

    serial_printf("[HEAP] Initialised: 0x%x - 0x%x (%u KB usable)\n",
                  HEAP_START, HEAP_END,
                  (HEAP_SIZE - sizeof(block_header_t)) / 1024);
}

/* ------------------------------------------------------------------ */
/* kmalloc                                                             */
/* Implements: REQ-MEM-04                                              */
/* ------------------------------------------------------------------ */
/*
 * First-fit allocator with block splitting.
 *
 * Algorithm:
 *   Walk the free list from heap_head.
 *   On finding a free block large enough:
 *     - If block is much larger than needed, split it:
 *         [header | requested size | header | remainder]
 *     - Mark block used, return pointer to data area.
 *   Return NULL if no block is large enough.
 *
 * The returned pointer points to the byte immediately after the header.
 */
void *kmalloc(uint32_t size)
{
    if (!heap_head) {
        serial_puts("[HEAP] ERROR: heap not initialised (call mm_heap_init first)\n");
        return 0;
    }

    /* Round up to minimum allocation size */
    if (size < MIN_ALLOC) size = MIN_ALLOC;

    block_header_t *cur = heap_head;

    while (cur) {
        /* Skip used blocks and blocks that are too small */
        if (cur->used || cur->size < size) {
            cur = cur->next;
            continue;
        }

        /*
         * Block is free and large enough.
         * Split if there's enough space for a new header + MIN_ALLOC bytes.
         */
        uint32_t leftover = cur->size - size;
        if (leftover > sizeof(block_header_t) + MIN_ALLOC) {
            /* Create a new free block in the leftover space */
            block_header_t *split =
                (block_header_t *)((uint8_t *)cur
                                   + sizeof(block_header_t)
                                   + size);
            split->size = leftover - sizeof(block_header_t);
            split->used = 0;
            split->next = cur->next;

            cur->size = size;
            cur->next = split;
        }

        cur->used = 1;
        return (uint8_t *)cur + sizeof(block_header_t);
    }

    serial_printf("[HEAP] ERROR: kmalloc(%u) - out of heap memory\n", size);
    return 0;
}

/* ------------------------------------------------------------------ */
/* kfree                                                               */
/* Implements: REQ-MEM-04                                              */
/* ------------------------------------------------------------------ */
/*
 * Marks the block as free, then coalesces adjacent free blocks
 * (forward merge only — sufficient for S2; bi-directional merge
 * can be added in S5 if heap fragmentation becomes a problem).
 *
 * The header is at ptr - sizeof(block_header_t).
 */
void kfree(void *ptr)
{
    if (!ptr) return;

    /* Recover the header immediately before the data pointer */
    block_header_t *hdr =
        (block_header_t *)((uint8_t *)ptr - sizeof(block_header_t));

    /* Sanity check: should be marked used */
    if (!hdr->used) {
        serial_printf("[HEAP] WARNING: kfree() called on already-free "
                      "block @ 0x%x\n", (uint32_t)ptr);
        return;
    }

    hdr->used = 0;

    /*
     * Coalesce: merge this block with consecutive free blocks.
     * Walk forward as long as the next block is also free.
     */
    block_header_t *cur = hdr;
    while (cur->next && !cur->next->used) {
        /* Absorb next block: add its header + data size to cur->size */
        cur->size += sizeof(block_header_t) + cur->next->size;
        cur->next  = cur->next->next;
    }
}

/* ------------------------------------------------------------------ */
/* Diagnostics (useful for S5 stress test)                            */
/* ------------------------------------------------------------------ */

/*
 * mm_heap_dump()
 *   Prints a summary of all heap blocks via serial.
 *   Call from kernel_main or GDB expression to inspect heap state.
 *
 *   BUG FIX: replaced "%5d" with "%d" — serial_printf only supports
 *   plain %d (no width specifier), so "%5d" was printed literally as
 *   the format string "size=%5d" instead of the actual block size.
 */
void mm_heap_dump(void)
{
    serial_puts("\n[HEAP] --- dump ---\n");
    block_header_t *cur = heap_head;
    int idx = 0;
    uint32_t total_free = 0, total_used = 0;

    while (cur) {
        serial_printf("  [%d] @ 0x%x  size=%d  %s\n",
                      idx++,
                      (uint32_t)cur,
                      cur->size,
                      cur->used ? "USED" : "free");
        if (cur->used) total_used += cur->size;
        else           total_free += cur->size;
        cur = cur->next;
    }

    serial_printf("  Total used: %u bytes | Total free: %u bytes\n",
                  total_used, total_free);
    serial_puts("[HEAP] --- end ---\n\n");
}