#ifndef PAGING_H
#define PAGING_H

/*
 * include/paging.h
 * Paging subsystem public API for MiniOS (S2)
 * Implements: REQ-MEM-02, REQ-MEM-05, EXT-SW-01
 *
 * Owner : Aman Yadav (B24CS1006)
 * Sprint: S2 — Virtual Memory & Kernel Heap
 */

#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Page flag bits (OR these into the flags argument)                   */
/* ------------------------------------------------------------------ */
#define PAGE_PRESENT    0x001   /* page is present in memory           */
#define PAGE_RW         0x002   /* read/write (0 = read-only)          */
#define PAGE_USER       0x004   /* user-accessible (0 = supervisor)    */
#define PAGE_ACCESSED   0x020   /* set by CPU on access                */
#define PAGE_DIRTY      0x040   /* set by CPU on write                 */

/* Convenience: kernel page = RW, supervisor-only (no PAGE_USER) */
#define PAGE_KERNEL     (PAGE_PRESENT | PAGE_RW)
/* User page = RW + accessible from ring-3 */
#define PAGE_USER_RW    (PAGE_PRESENT | PAGE_RW | PAGE_USER)

/* ------------------------------------------------------------------ */
/* Types                                                               */
/* ------------------------------------------------------------------ */

/*
 * A page directory holds 1024 × 4-byte entries.
 * A page table   holds 1024 × 4-byte entries.
 * Both occupy exactly one 4KB frame.
 */
typedef uint32_t page_dir_t[1024];
typedef uint32_t page_table_t[1024];

/* ------------------------------------------------------------------ */
/* Core paging API (arch/x86/paging.c + arch/x86/paging_asm.asm)     */
/* ------------------------------------------------------------------ */

/*
 * paging_create_directory()
 *   Allocates one 4KB-aligned frame for a page directory via
 *   mm_alloc_frame(), zeroes all 1024 entries (not-present),
 *   and returns a pointer to it.
 *   Returns NULL on frame exhaustion.
 *   Implements: REQ-MEM-02
 */
page_dir_t *paging_create_directory(void);

/*
 * paging_free_directory(dir)
 *   Frees every present page table frame referenced by dir,
 *   then frees the directory frame itself via mm_free_frame().
 *   Called by task_destroy() in S4.
 *   Implements: EXT-SW-01
 */
void paging_free_directory(page_dir_t *dir);

/*
 * paging_map_page(dir, virt, phys, flags)
 *   Maps one 4KB virtual page → one 4KB physical frame inside dir.
 *   - dir   : target page directory (physical address, pre-paging)
 *   - virt  : virtual address (must be 4KB-aligned)
 *   - phys  : physical address (must be 4KB-aligned)
 *   - flags : combination of PAGE_* constants (PAGE_PRESENT added auto)
 *   Allocates a new page table frame if the PD slot is empty.
 *   Implements: REQ-MEM-02
 */
void paging_map_page(page_dir_t *dir,
                     uint32_t    virt,
                     uint32_t    phys,
                     uint32_t    flags);

/*
 * paging_identity_map_first4mb(dir)
 *   Maps virt 0x00000000–0x003FFFFF → phys 0x00000000–0x003FFFFF.
 *   Required before enabling paging so the CPU can still fetch
 *   instructions at their current physical addresses.
 *   Implements: REQ-MEM-02
 */
void paging_identity_map_first4mb(page_dir_t *dir);

/*
 * paging_map_kernel(dir)
 *   Maps virt 0xC0000000–0xC03FFFFF → phys 0x00100000–0x004FFFFF.
 *   Uses supervisor-only flags (no PAGE_USER) for kernel isolation.
 *   Implements: REQ-MEM-02, REQ-MEM-05
 */
void paging_map_kernel(page_dir_t *dir);

/* ------------------------------------------------------------------ */
/* NASM-implemented helpers (arch/x86/paging_asm.asm)                 */
/* These write to CR3 and CR0 — MUST stay in arch/x86/ (NFR-MAINT-01) */
/* ------------------------------------------------------------------ */

/*
 * paging_load_directory(phys_addr)
 *   Loads phys_addr into CR3 (PDBR register).
 *   Implements: REQ-MEM-02
 */
void paging_load_directory(uint32_t phys_addr);

/*
 * paging_enable()
 *   Sets bit 31 (PG) of CR0 to activate paging.
 *   Call ONLY after CR3 is loaded and all required mappings are set.
 *   Implements: REQ-MEM-02
 */
void paging_enable(void);

/* ------------------------------------------------------------------ */
/* Page fault handler (arch/x86/isr14.c)                              */
/* ------------------------------------------------------------------ */

/*
 * isr_14_handler(err_code)
 *   Called from the NASM ISR-14 stub (arch/x86/isr_stubs.asm).
 *   Reads CR2 for the faulting address, logs details via serial,
 *   then halts (hlt loop) per NFR-REL-01.
 *   Implements: REQ-MEM-05, NFR-REL-01
 */
void isr_14_handler(uint32_t err_code);

uint32_t paging_get_cr2(void);
uint32_t paging_get_cr3(void);

#endif /* PAGING_H */
