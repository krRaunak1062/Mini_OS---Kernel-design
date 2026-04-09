/*
 * kernel/kernel_main.c
 * Main kernel entry point — Sprint 1 + Sprint 2 merged.
 *
 * Implements:
 *   S1 : REQ-MEM-01 (GDT), REQ-MEM-03 (physical allocator)
 *   S2 : REQ-MEM-02, REQ-MEM-04, REQ-MEM-05 (paging + heap)
 *
 * Authors:
 *   Shardul Diwate  (B24CS1028) — boot scaffolding (S0)
 *   Aman Yadav      (B24CS1006) — memory subsystem calls (S1, S2)
 *
 * Call order (critical — DO NOT reorder):
 *   1.  serial_init()                   S0 : COM1 logger
 *   2.  vga_init()                      S0 : VGA text driver
 *   3.  gdt_init()                      S1 : stable segment descriptors
 *   4.  mm_init()                       S1 : bitmap allocator from mmap
 *   5.  isr_handlers_init()             S2 : register ISR 13 + ISR 14
 *   6.  idt_load_minimal()              S2 : load minimal IDT (ISR 14)
 *   7.  paging_create_directory()       S2 : alloc kernel page directory
 *   8.  paging_identity_map_first4mb()  S2 : must be before paging_enable
 *   9.  paging_map_kernel()             S2 : higher-half kernel mapping
 *  10.  paging_map_heap()               S2 : map heap region
 *  11.  paging_load_directory()         S2 : load CR3
 *  12.  paging_enable()                 S2 : set CR0.PG  ← PAGING ON
 *  13.  mm_heap_init()                  S2 : initialise heap free list
 *  14.  test_s2()                       S2 : verification tests
 *
 * NOTE: The S1 frame self-test (alloc/free 3 frames) is preserved
 *       as a lightweight sanity check that runs before paging is on,
 *       then S2 picks up from step 5 onward.
 */

#include <stdint.h>
#include "vga.h"           /* S0 */
#include "serial.h"        /* S0 */
#include "gdt.h"           /* S1: REQ-MEM-01 */
#include "mm_phys.h"       /* S1: REQ-MEM-03  — mm_init / mm_alloc_frame / mm_free_frame */
#include "multiboot.h"     /* S1: multiboot_info_t */
#include "mm_heap.h"       /* S2: REQ-MEM-04 — mm_heap_init / kmalloc / kfree / mm_heap_dump */
#include "paging.h"        /* S2: REQ-MEM-02 — page_dir_t / paging_* helpers */

/* ------------------------------------------------------------------ */
/* External symbols                                                    */
/* ------------------------------------------------------------------ */
extern void idt_load_minimal(void);   /* arch/x86/idt_minimal.c  (S2) */
extern void isr_handlers_init(void);  /* arch/x86/isr.c          (S2) */

/* Multiboot magic number that GRUB puts in EAX */
#define MULTIBOOT_MAGIC 0x2BADB002

/* ------------------------------------------------------------------ */
/* Heap mapping helper                                      (S2)       */
/* ------------------------------------------------------------------ */
/*
 * paging_map_kernel() maps virt 0xC0000000–0xC03FFFFF (4 MB kernel).
 * The heap lives at 0xC0400000 for 4 MB and must be mapped separately
 * with the same supervisor-only, read-write flags.
 */
static void paging_map_heap(page_dir_t *dir)
{
    serial_puts("[KERNEL] Mapping heap region 0xC0400000-0xC07FFFFF...\n");

    uint32_t virt = 0xC0400000;
    uint32_t phys = 0x00500000;   /* physical memory after kernel + identity */

    /* 4 MB = 1024 pages × 4 KB */
    for (uint32_t i = 0; i < 1024; i++) {
        paging_map_page(dir, virt, phys, PAGE_RW);   /* supervisor-only */
        virt += 0x1000;
        phys += 0x1000;
    }

    serial_puts("[KERNEL] Heap region mapped.\n");
}

/* ------------------------------------------------------------------ */
/* Sprint-2 verification tests                              (S2)       */
/* ------------------------------------------------------------------ */
static void test_s2(void)
{
    serial_puts("\n==============================\n");
    serial_puts("  S2 VERIFICATION TESTS\n");
    serial_puts("==============================\n");

    /* ---- Test 1: paging active ---- */
    serial_puts("[TEST 1] Paging active check...\n");
    uint32_t cr0;
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
    if (cr0 & 0x80000000) {
        serial_puts("  PASS: CR0.PG is set\n");
    } else {
        serial_puts("  FAIL: CR0.PG is NOT set\n");
    }

    /* ---- Test 2: CR3 value ---- */
    serial_printf("[TEST 2] CR3 = 0x%x\n", paging_get_cr3());

    /* ---- Test 3: kmalloc basic ---- */
    serial_puts("[TEST 3] kmalloc basic...\n");
    void *p1 = kmalloc(64);
    void *p2 = kmalloc(128);
    void *p3 = kmalloc(256);
    if (p1 && p2 && p3) {
        serial_printf("  PASS: p1=0x%x p2=0x%x p3=0x%x\n",
                      (uint32_t)p1, (uint32_t)p2, (uint32_t)p3);
    } else {
        serial_puts("  FAIL: kmalloc returned NULL\n");
    }

    /* ---- Test 4: kfree and reuse ---- */
    serial_puts("[TEST 4] kfree and realloc...\n");
    kfree(p2);
    void *p4 = kmalloc(128);
    if (p4 == p2) {
        serial_puts("  PASS: freed block was reused\n");
    } else {
        serial_printf("  INFO: p4=0x%x (different from p2=0x%x — "
                      "coalesced with neighbour)\n",
                      (uint32_t)p4, (uint32_t)p2);
    }
    kfree(p1);
    kfree(p3);
    kfree(p4);

    /* ---- Test 5: stress — 100 alloc / free cycles ---- */
    serial_puts("[TEST 5] Stress: 100 allocs of 64 bytes...\n");
    void *ptrs[100];
    int ok = 1;
    for (int i = 0; i < 100; i++) {
        ptrs[i] = kmalloc(64);
        if (!ptrs[i]) { ok = 0; break; }
    }
    for (int i = 0; i < 100; i++) {
        if (ptrs[i]) kfree(ptrs[i]);
    }
    serial_printf("  %s: 100 alloc/free cycle\n", ok ? "PASS" : "FAIL");

    /* ---- Test 6: heap dump ---- */
    mm_heap_dump();

    /* ---- Test 7: intentional page-fault (disabled by default) ----
     *
     * Uncomment to verify ISR 14 fires correctly.
     * The kernel will halt after printing the fault details.
     *
     *   serial_puts("[TEST 7] Triggering intentional page fault...\n");
     *   volatile uint32_t *bad = (uint32_t *)0xDEAD0000;
     *   uint32_t val = *bad;
     *   (void)val;
     */

    /* ---- Test 8: paging_create / free_directory (EXT-SW-01) ---- */
    serial_puts("[TEST 8] paging_create/free_directory (EXT-SW-01)...\n");
    page_dir_t *task_dir = paging_create_directory();
    if (task_dir) {
        serial_printf("  Created task PD @ phys 0x%x\n", (uint32_t)task_dir);
        paging_free_directory(task_dir);
        serial_puts("  PASS: create and free succeeded\n");
    } else {
        serial_puts("  FAIL: paging_create_directory() returned NULL\n");
    }

    serial_puts("==============================\n");
    serial_puts("  S2 TESTS COMPLETE\n");
    serial_puts("==============================\n\n");
}

/* ================================================================== */
/*  kernel_main                                                        */
/* ================================================================== */
/*
 * Parameter order matches what boot.asm pushes on the stack:
 *   push ebx   (multiboot info pointer — second push)
 *   push eax   (magic number          — first push)
 * so C sees: kernel_main(magic, mb_info)  ← correct order.
 *
 * mb_info is the raw physical address GRUB put in EBX.
 * We cast it to multiboot_info_t* once the magic check passes.
 * It must be used as a physical pointer BEFORE paging is enabled.
 */
void kernel_main(uint32_t magic, uint32_t mb_info)
{
    /* ── S0: early I/O ──────────────────────────────────────────── */
    serial_init();
    serial_puts("[BOOT] Serial logger initialized\n");

    vga_init();
    serial_puts("[BOOT] VGA driver initialized\n");

    /* Verify multiboot magic before touching the mb_info pointer */
    if (magic != MULTIBOOT_MAGIC) {
        vga_set_color(VGA_COLOR_RED, VGA_COLOR_BLACK);
        vga_puts("ERROR: Invalid Multiboot magic number!\n");
        serial_puts("[ERROR] Invalid Multiboot magic number!\n");
        goto hang;
    }

    serial_puts("\n[KERNEL] MiniOS — Sprint 1 + Sprint 2\n");
    serial_printf("[KERNEL] Multiboot magic: 0x%x\n", magic);

    /* ── S1: GDT ────────────────────────────────────────────────── */
    /*
     * Must come first. CPU segment registers still hold whatever GRUB
     * left — gdt_init() loads our 3-entry flat GDT and flushes all
     * segment registers (CS via far-jump, DS/ES/FS/GS/SS directly).
     */
    serial_puts("[KERNEL] Initialising GDT...\n");
    gdt_init();
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("[OK] GDT loaded (null + kernel code + kernel data)\n");
    serial_puts("[S1]  GDT initialized\n");

    /* ── S1: Physical memory allocator ─────────────────────────── */
    /*
     * Cast mb_info to multiboot_info_t* — still the physical address
     * GRUB put in EBX.  mm_init() walks mbi->mmap_addr to find usable
     * RAM and builds our 4-KB-frame bitmap.
     * Must happen before paging (S2) because the pointer is physical.
     */
    multiboot_info_t *mbi = (multiboot_info_t *)mb_info;
    serial_puts("[KERNEL] Initialising physical memory manager...\n");
    mm_init(mbi);
    vga_puts("[OK] Physical memory allocator ready\n");
    serial_puts("[S1]  mm_init() complete\n");

    /* ── S1: Frame self-test ────────────────────────────────────── */
    /*
     * Alloc and immediately free 3 frames as a lightweight sanity
     * check.  Runs before paging is on so the addresses are physical.
     * Safe to leave in; the frames are returned to the free pool.
     */
    uint32_t f1 = mm_alloc_frame();
    uint32_t f2 = mm_alloc_frame();
    uint32_t f3 = mm_alloc_frame();
    serial_puts("[S1]  test alloc: ");
    serial_puts_hex(f1); serial_puts(" ");
    serial_puts_hex(f2); serial_puts(" ");
    serial_puts_hex(f3); serial_puts("\n");
    mm_free_frame(f1);
    mm_free_frame(f2);
    mm_free_frame(f3);
    serial_puts("[S1]  test free: OK\n");

    /* ── S2: ISR handler table ──────────────────────────────────── */
    /*
     * Register stub handlers for ISR 13 (GPF) and ISR 14 (#PF).
     * Must be done before idt_load_minimal() installs the IDT gate,
     * and certainly before paging_enable() could trigger a #PF.
     */
    isr_handlers_init();

    /* ── S2: Minimal IDT ────────────────────────────────────────── */
    /*
     * Load a minimal IDT containing at least ISR 14 so a page fault
     * during paging bring-up doesn't triple-fault the CPU.
     * Full 256-entry IDT is Raunak's S3 work.
     */
    serial_puts("[KERNEL] Loading minimal IDT (ISR 13, ISR 14)...\n");
    idt_load_minimal();
    serial_puts("[KERNEL] IDT loaded.\n");

    /* ── S2: Paging setup ───────────────────────────────────────── */
    serial_puts("[KERNEL] Setting up paging...\n");

    /* Allocate the kernel page directory */
    page_dir_t *kernel_dir = paging_create_directory();
    if (!kernel_dir) {
        serial_puts("[KERNEL] FATAL: could not allocate page directory\n");
        goto hang;
    }

    /* 1. Identity-map first 4 MB (required before CR0.PG is set so
     *    that the next instruction after paging_enable() still maps
     *    to the same physical address we are currently executing). */
    paging_identity_map_first4mb(kernel_dir);

    /* 2. Map kernel to higher half: virt 0xC0000000 → phys 0x00100000 */
    paging_map_kernel(kernel_dir);

    /* 3. Map heap region: virt 0xC0400000 → phys 0x00500000 (4 MB) */
    paging_map_heap(kernel_dir);

    /* 4. Load CR3 with the physical address of our page directory */
    paging_load_directory((uint32_t)kernel_dir);

    /* 5. Enable paging — CR0.PG set — all accesses now translated */
    paging_enable();

    vga_puts("[OK] Paging enabled\n");
    serial_puts("[KERNEL] Paging ENABLED.\n");

    /* ── S2: Kernel heap ────────────────────────────────────────── */
    /*
     * mm_heap_init() MUST be called AFTER paging_enable() because
     * the heap base pointer (0xC0400000) is a virtual address that
     * only exists once the heap pages are mapped and paging is on.
     */
    mm_heap_init();
    vga_puts("[OK] Kernel heap initialized\n");
    serial_puts("[S2]  mm_heap_init() complete\n");

    /* ── Boot banner ────────────────────────────────────────────── */
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("=======================================\n");
    vga_puts("       MiniOS Kernel v0.2              \n");
    vga_puts("       Group 31 | IIT Jodhpur          \n");
    vga_puts("=======================================\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("[OK] VGA driver active\n");
    vga_puts("[OK] Serial logger active (COM1 9600 baud)\n");
    vga_puts("[OK] Protected mode confirmed\n");
    vga_puts("[OK] GDT initialized\n");
    vga_puts("[OK] Physical allocator ready\n");
    vga_puts("[OK] Paging enabled\n");
    vga_puts("[OK] Kernel heap initialized\n");
    vga_puts("\nSprint 1 + Sprint 2 complete. Ready for S3.\n");

    /* ── S2: Run verification tests ─────────────────────────────── */
    test_s2();

    /* ── Hand off to S3 (Raunak) ────────────────────────────────── */
    serial_puts("[KERNEL] S2 complete. Ready for S3 (IDT + PIC + PIT).\n");

hang:
    /* Spin loop — scheduler takes over in S4 */
    while (1) {
        __asm__ volatile ("hlt");
    }
}