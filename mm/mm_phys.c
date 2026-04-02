/* Implements: REQ-MEM-03 */
#include <stdint.h>        /* uintptr_t, uint32_t, etc. — MUST be first */
#include "mm_phys.h"
#include "multiboot.h"
#include "serial.h"   /* your serial_log() from S0 */



#define PAGE_SIZE   4096
#define MAX_FRAMES  (1024 * 1024)          /* 4GB / 4KB = 1M frames */

/*
 * The bitmap. One bit per frame. 1M bits = 32768 uint32_t words.
 * This lives in BSS (zero-initialised by your linker/boot stub).
 * At 128KB it's a bit large — but fine for a kernel-space static array.
 */
static uint32_t bitmap[MAX_FRAMES / 32];

/* ── helpers ─────────────────────────────────────────── */

static void mm_set_frame(uint32_t addr) {
    uint32_t frame = addr / PAGE_SIZE;
    bitmap[frame / 32] |= (1u << (frame % 32));
}

static void mm_clear_frame(uint32_t addr) {
    uint32_t frame = addr / PAGE_SIZE;
    bitmap[frame / 32] &= ~(1u << (frame % 32));
}

static int mm_test_frame(uint32_t addr) {
    uint32_t frame = addr / PAGE_SIZE;
    return (bitmap[frame / 32] >> (frame % 32)) & 1;
}

/* ── mm_init ─────────────────────────────────────────── */

void mm_init(multiboot_info_t *mbi)
{
    /*
     * Step 1: Mark everything as allocated.
     * Safe default — we'll explicitly free only what GRUB says is usable.
     */
    for (uint32_t i = 0; i < MAX_FRAMES / 32; i++)
        bitmap[i] = 0xFFFFFFFF;

    /*
     * Step 2: Check the Multiboot flags.
     * Bit 6 of mbi->flags means the memory map is present.
     * If it's not there, we can't safely know what's usable — halt.
     */
    if (!(mbi->flags & (1 << 6))) {
        serial_log("[mm_init] ERROR: no Multiboot memory map!\n");
        for(;;);
    }

    /*
     * Step 3: Walk the Multiboot memory map.
     * Each entry describes a region: addr, len, type.
     * type=1 means AVAILABLE (usable RAM).
     * Note: mmap entries have a 'size' field that you use to advance,
     * not sizeof(multiboot_memory_map_t) — they can vary in length.
     */
    multiboot_memory_map_t *mmap =
        (multiboot_memory_map_t *)(uintptr_t)mbi->mmap_addr;

    multiboot_memory_map_t *mmap_end =
        (multiboot_memory_map_t *)(uintptr_t)(mbi->mmap_addr + mbi->mmap_length);

    while (mmap < mmap_end) {

        if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {

            /* Walk every 4KB frame in this usable region */
            uint64_t addr = mmap->addr;
            uint64_t end  = mmap->addr + mmap->len;

            while (addr + PAGE_SIZE <= end) {

                /*
                 * Skip the low 1MB entirely.
                 * It contains VGA, BIOS, interrupt tables.
                 * Even if GRUB says some of it is "available",
                 * touching it is dangerous without extra care.
                 */
                if (addr >= 0x100000)
                    mm_clear_frame((uint32_t)addr);   /* mark as free */

                addr += PAGE_SIZE;
            }
        }

        /* Advance to next entry — use size field, not sizeof */
        mmap = (multiboot_memory_map_t *)
               ((uintptr_t)mmap + mmap->size + sizeof(mmap->size));
    }

    /*
     * Step 4: Re-mark the kernel's own pages as allocated.
     * GRUB reported the region starting at 0x100000 as AVAILABLE,
     * but we loaded our kernel there! We must protect it.
     *
     * kernel_start and kernel_end are symbols exported by the linker script.
     * Add these to your linker.ld:
     *   kernel_start = .;   (before .text)
     *   kernel_end   = .;   (after .bss)
     */
    extern uint32_t kernel_start, kernel_end;
    uint32_t ks = (uint32_t)&kernel_start & ~(PAGE_SIZE - 1);  /* round down */
    uint32_t ke = ((uint32_t)&kernel_end  + PAGE_SIZE - 1)     /* round up   */
                    & ~(PAGE_SIZE - 1);

    for (uint32_t a = ks; a < ke; a += PAGE_SIZE)
        mm_set_frame(a);

    serial_log("[mm_init] ready. Kernel: 0x%x – 0x%x\n", ks, ke);
}

/* ── mm_alloc_frame ──────────────────────────────────── */

/*
 * Find the first free frame, mark it allocated, return its physical address.
 * Returns 0 on exhaustion (frame 0 is always reserved anyway).
 *
 * Implements: REQ-MEM-03
 */
uint32_t mm_alloc_frame(void)
{
    for (uint32_t word = 0; word < MAX_FRAMES / 32; word++) {

        /* Fast skip: if all 32 bits set, no free frames in this word */
        if (bitmap[word] == 0xFFFFFFFF)
            continue;

        /* Find the first clear bit in this word */
        for (uint32_t bit = 0; bit < 32; bit++) {
            if (!(bitmap[word] & (1u << bit))) {
                bitmap[word] |= (1u << bit);                /* mark allocated */
                return (word * 32 + bit) * PAGE_SIZE;       /* physical addr  */
            }
        }
    }

    /* No free frames left */
    serial_log("[mm_alloc_frame] OUT OF MEMORY\n");
    return 0;
}

/* ── mm_free_frame ───────────────────────────────────── */

/*
 * Release a frame back to the pool.
 * Validates alignment and guards against double-free.
 *
 * Implements: REQ-MEM-03
 */
void mm_free_frame(uint32_t addr)
{
    /* Guard: address must be 4KB-aligned */
    if (addr % PAGE_SIZE != 0) {
        serial_log("[mm_free_frame] misaligned: 0x%x\n", addr);
        return;
    }

    /* Guard: don't free a frame that isn't allocated (double-free) */
    if (!mm_test_frame(addr)) {
        serial_log("[mm_free_frame] double-free: 0x%x\n", addr);
        return;
    }

    mm_clear_frame(addr);
}

void mm_test(void)
{
    serial_log("[mm_test] starting...\n");

    uint32_t frames[100];
    for (int i = 0; i < 100; i++) {
        frames[i] = mm_alloc_frame();
        if (frames[i] == 0) {
            serial_log("[mm_test] FAIL: alloc returned 0 at i=%d\n", i);
            return;
        }
        /* Every frame must be 4KB-aligned */
        if (frames[i] % 4096 != 0) {
            serial_log("[mm_test] FAIL: misaligned 0x%x at i=%d\n", frames[i], i);
            return;
        }
        /* Consecutive allocs should be consecutive frames */
        if (i > 0 && frames[i] != frames[i-1] + 4096) {
            serial_log("[mm_test] NOTE: gap at i=%d (0x%x -> 0x%x)\n",
                       i, frames[i-1], frames[i]);
        }
    }
    serial_log("[mm_test] alloc OK, freeing...\n");

    for (int i = 0; i < 100; i++)
        mm_free_frame(frames[i]);

    /* After freeing, re-alloc should give back the same first address */
    uint32_t check = mm_alloc_frame();
    if (check == frames[0])
        serial_log("[mm_test] PASS: bitmap integrity confirmed\n");
    else
        serial_log("[mm_test] WARN: got 0x%x, expected 0x%x\n", check, frames[0]);

    mm_free_frame(check);
    serial_log("[mm_test] done.\n");
}