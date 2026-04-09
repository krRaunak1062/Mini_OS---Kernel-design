/*
 * arch/x86/paging.c
 * Two-level x86 paging subsystem for MiniOS.
 *
 * Implements: REQ-MEM-02 (virtual memory mapping)
 *             REQ-MEM-05 (kernel-user isolation)
 *             EXT-SW-01  (paging_create/free_directory for scheduler)
 *
 * Owner : Aman Yadav (B24CS1006)
 * Sprint: S2 — Virtual Memory & Kernel Heap
 *
 * Design:
 *   x86 uses a two-level page table:
 *     bits [31:22] → Page Directory index  (1024 entries)
 *     bits [21:12] → Page Table index      (1024 entries per table)
 *     bits [11: 0] → Page offset           (4096 bytes)
 *
 *   Kernel virtual base : 0xC0000000  (PD index 768)
 *   Kernel physical base: 0x00100000
 *   Identity map        : 0x00000000 – 0x003FFFFF (first 4MB)
 *
 * IMPORTANT: paging_load_directory() and paging_enable() (CR3/CR0 writes)
 * are implemented in arch/x86/paging_asm.asm per NFR-MAINT-01.
 */

#include "paging.h"
#include "mm_phys.h"
#include "mm_heap.h"
#include "serial.h"
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Internal helpers                                                     */
/* ------------------------------------------------------------------ */

/*
 * pd_index(virt) — extract bits [31:22]
 * pt_index(virt) — extract bits [21:12]
 */
#define PD_INDEX(virt)  ((virt) >> 22)
#define PT_INDEX(virt)  (((virt) >> 12) & 0x3FF)

/* Strip flag bits from a PD/PT entry to get the frame address */
#define ENTRY_ADDR(e)   ((e) & ~0xFFFU)

/* ------------------------------------------------------------------ */
/* paging_create_directory                                             */
/* Implements: REQ-MEM-02                                              */
/* ------------------------------------------------------------------ */
/*
 * Allocates one 4KB frame from the physical bitmap allocator and
 * zeroes all 1024 entries so every slot is "not present".
 * Returns a pointer (== physical address before paging is enabled).
 */
page_dir_t *paging_create_directory(void)
{
    uint32_t phys = mm_alloc_frame();
    if (!phys) {
        serial_puts("[PAGING] ERROR: mm_alloc_frame() returned 0 "
                    "in paging_create_directory\n");
        return 0;
    }

    page_dir_t *dir = (page_dir_t *)phys;

    /* Zero all 1024 entries — sets not-present for every PD slot */
    uint32_t *raw = (uint32_t *)dir;
    for (int i = 0; i < 1024; i++) {
        raw[i] = 0;
    }

    serial_printf("[PAGING] Created page directory @ phys 0x%x\n", phys);
    return dir;
}

/* ------------------------------------------------------------------ */
/* paging_free_directory                                               */
/* Implements: EXT-SW-01                                               */
/* ------------------------------------------------------------------ */
/*
 * Iterates every PD entry; for each present entry (i.e. an allocated
 * page table), frees the page table frame via mm_free_frame().
 * Finally frees the page directory frame itself.
 *
 * Called by task_destroy() in S4 (scheduler) after a task exits.
 * NOTE: Does NOT unmap kernel pages (PD index 768+) — those are shared
 *       across all address spaces and must not be freed here.
 */
void paging_free_directory(page_dir_t *dir)
{
    if (!dir) return;

    uint32_t *raw = (uint32_t *)dir;

    /*
     * Only free user-space page tables (indices 0–767).
     * Kernel mappings (768–1023) are shared; freeing them would
     * corrupt every other task's address space.
     */
    for (int i = 0; i < 768; i++) {
        if (raw[i] & PAGE_PRESENT) {
            uint32_t pt_phys = ENTRY_ADDR(raw[i]);
            mm_free_frame(pt_phys);
            serial_printf("[PAGING] Freed page table @ phys 0x%x "
                          "(PD[%d])\n", pt_phys, i);
        }
    }

    /* Free the page directory frame */
    mm_free_frame((uint32_t)dir);
    serial_printf("[PAGING] Freed page directory @ phys 0x%x\n",
                  (uint32_t)dir);
}

/* ------------------------------------------------------------------ */
/* paging_map_page                                                     */
/* Implements: REQ-MEM-02                                              */
/* ------------------------------------------------------------------ */
/*
 * Maps one 4KB virtual page to one 4KB physical frame inside `dir`.
 *
 * Algorithm:
 *   1. Compute PD index (bits 31:22) and PT index (bits 21:12).
 *   2. If PD entry is not-present, allocate a new page table frame,
 *      zero it, and write it into the PD entry.
 *   3. Write the physical frame address + flags into the PT entry.
 *
 * PAGE_PRESENT is always OR-ed in by this function — callers do not
 * need to include it in flags.
 */
void paging_map_page(page_dir_t *dir,
                     uint32_t    virt,
                     uint32_t    phys,
                     uint32_t    flags)
{
    uint32_t pdi = PD_INDEX(virt);
    uint32_t pti = PT_INDEX(virt);
    uint32_t *raw_dir = (uint32_t *)dir;

    /* ---- Step 1: ensure page table exists ---- */
    if (!(raw_dir[pdi] & PAGE_PRESENT)) {
        uint32_t pt_phys = mm_alloc_frame();
        if (!pt_phys) {
            serial_printf("[PAGING] ERROR: out of frames mapping "
                          "virt 0x%x\n", virt);
            return;
        }

        /* Zero the new page table */
        uint32_t *pt_raw = (uint32_t *)pt_phys;
        for (int i = 0; i < 1024; i++) {
            pt_raw[i] = 0;
        }

        /*
         * Write PD entry.
         * Inherit flags from the mapping request so that user-space
         * PTs are marked PAGE_USER at the directory level too.
         * PAGE_RW is always set on the PD entry to allow the PT
         * to control per-page write permissions.
         */
        raw_dir[pdi] = pt_phys | PAGE_RW | PAGE_PRESENT
                       | (flags & PAGE_USER);
    }

    /* ---- Step 2: write page table entry ---- */
    /*
     * Recover the PT frame address by masking off the low 12 flag bits.
     * This is the most common bug here — using the raw entry as a
     * pointer includes the flag bits and gives a wrong address.
     */
    uint32_t *pt_raw = (uint32_t *)ENTRY_ADDR(raw_dir[pdi]);

    /* Align the physical address (defensive) and set flags + present */
    pt_raw[pti] = (phys & ~0xFFFU) | flags | PAGE_PRESENT;
}

/* ------------------------------------------------------------------ */
/* paging_identity_map_first4mb                                        */
/* Implements: REQ-MEM-02                                              */
/* ------------------------------------------------------------------ */
/*
 * Maps virt 0x00000000–0x003FFFFF → phys 0x00000000–0x003FFFFF.
 *
 * WHY THIS IS REQUIRED:
 *   Before paging is enabled, the CPU fetches instructions using
 *   physical addresses (e.g., 0x00100000 for the kernel entry point).
 *   The moment CR0.PG is set, every address is translated through the
 *   page tables. Without an identity map the very next instruction
 *   fetch after the CR0 write faults — causing a triple fault.
 *
 *   The identity map makes phys == virt for the first 4MB so those
 *   instruction fetches succeed until the CPU begins using the
 *   higher-half virtual addresses (0xC0000000+).
 *
 * This map can be removed later (once all code runs from 0xC0000000+),
 * but for S2 leave it in place.
 */
void paging_identity_map_first4mb(page_dir_t *dir)
{
    serial_puts("[PAGING] Identity-mapping first 4MB...\n");

    for (uint32_t addr = 0; addr < 0x400000; addr += 0x1000) {
        paging_map_page(dir, addr, addr, PAGE_RW);
    }

    serial_puts("[PAGING] Identity map done (0x0 – 0x3FFFFF)\n");
}

/* ------------------------------------------------------------------ */
/* paging_map_kernel                                                   */
/* Implements: REQ-MEM-02, REQ-MEM-05                                  */
/* ------------------------------------------------------------------ */
/*
 * Maps the kernel into the higher half of every address space:
 *   virt 0xC0000000 – 0xC03FFFFF  →  phys 0x00100000 – 0x004FFFFF
 *
 * Supervisor-only (no PAGE_USER flag):
 *   A ring-3 (user mode) access to these pages will generate a
 *   page fault (ISR 14) — this enforces NFR-SEC-01 kernel isolation.
 *
 * PAGE_RW without PAGE_USER = Supervisor Read/Write.
 * The CPU checks the U/S bit on BOTH the PD entry and the PT entry.
 * paging_map_page() propagates the PAGE_USER flag to the PD entry
 * only when the caller passes it; here we never pass PAGE_USER.
 */
void paging_map_kernel(page_dir_t *dir)
{
    serial_puts("[PAGING] Mapping kernel at 0xC0000000...\n");

    uint32_t phys = 0x00100000;  /* kernel physical load address */
    uint32_t virt = 0xC0000000;  /* kernel virtual base           */

    /* Map 4MB = 1024 pages of 4KB each */
    for (uint32_t i = 0; i < 1024; i++) {
        /* PAGE_RW only — PAGE_USER deliberately omitted */
        paging_map_page(dir, virt, phys, PAGE_RW);
        virt += 0x1000;
        phys += 0x1000;
    }

    serial_printf("[PAGING] Kernel mapped: virt 0xC0000000 – 0xC03FFFFF"
                  " → phys 0x00100000 – 0x004FFFFF\n");
}
