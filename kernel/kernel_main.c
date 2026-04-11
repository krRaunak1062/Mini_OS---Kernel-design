/*
 * kernel/kernel_main.c
 * Main kernel entry point — Sprint 1 + Sprint 2 + Sprint 3 merged.
 *
 * Implements:
 *   S1 : REQ-MEM-01 (GDT), REQ-MEM-03 (physical allocator)
 *   S2 : REQ-MEM-02, REQ-MEM-04, REQ-MEM-05 (paging + heap)
 *   S3 : REQ-TASK-01 (IDT), REQ-TASK-02 (PIC + PIT + IRQ handlers)
 *
 * Authors:
 *   Shardul Diwate  (B24CS1028) — boot scaffolding (S0), build (S6)
 *   Aman Yadav      (B24CS1006) — memory subsystem calls (S1, S2)
 *   Raunak Kumar    (B24CS1062) — interrupt infrastructure (S3)
 *
 * Call order (critical — DO NOT reorder):
 *   1.  serial_init()                   S0 : COM1 logger
 *   2.  vga_init()                      S0 : VGA text driver
 *   3.  gdt_init()                      S1 : stable segment descriptors
 *   4.  mm_init()                       S1 : bitmap allocator from mmap
 *   5.  paging_create_directory()       S2 : alloc kernel page directory
 *   6.  paging_identity_map_first4mb()  S2 : must be before paging_enable
 *   7.  paging_map_kernel()             S2 : higher-half kernel mapping
 *   8.  paging_map_heap()               S2 : map heap region
 *   9.  paging_load_directory()         S2 : load CR3
 *  10.  paging_enable()                 S2 : set CR0.PG  ← PAGING ON
 *  11.  mm_heap_init()                  S2 : initialise heap free list
 *  12.  idt_init()                      S3 : build + load 256-entry IDT
 *  13.  pic_remap()                     S3 : remap IRQs to 0x20-0x2F
 *  14.  pit_init()                      S3 : PIT channel 0 at 100 Hz
 *  15.  isr_install()                   S3 : confirm C handlers active
 *  16.  STI                             S3 : enable interrupts
 *  17.  test_s2()                       S2 : heap/paging verification
 *  18.  test_s3()                       S3 : interrupt verification
 *
 * NOTE: The S2 minimal IDT (idt_load_minimal / isr_handlers_init) is
 *       fully replaced by S3's 256-entry IDT.  Remove idt_minimal.c
 *       from your Makefile OBJS — keeping both causes a linker error.
 */

#include <stdint.h>
#include "vga.h"           /* S0 */
#include "serial.h"        /* S0 */
#include "gdt.h"           /* S1: REQ-MEM-01 */
#include "mm_phys.h"       /* S1: REQ-MEM-03 — mm_init / mm_alloc_frame / mm_free_frame */
#include "multiboot.h"     /* S1: multiboot_info_t */
#include "mm_heap.h"       /* S2: REQ-MEM-04 — mm_heap_init / kmalloc / kfree / mm_heap_dump */
#include "paging.h"        /* S2: REQ-MEM-02 — page_dir_t / paging_* helpers */
#include "idt.h"           /* S3: REQ-TASK-01 — idt_init / idt_set_gate */
#include "isr.h"           /* S3: REQ-TASK-01, REQ-TASK-02 — isr_install / handlers */
#include "pic.h"           /* S3: REQ-TASK-02 — pic_remap / pic_send_eoi */
#include "pit.h"           /* S3: REQ-TASK-02 — pit_init / pit_get_ticks */

/* Multiboot magic number that GRUB puts in EAX */
#define MULTIBOOT_MAGIC 0x2BADB002

/* ------------------------------------------------------------------ */
/* Heap mapping helper                                      (S2)       */
/* ------------------------------------------------------------------ */
static void paging_map_heap(page_dir_t *dir)
{
    serial_puts("[KERNEL] Mapping heap region 0xC0400000-0xC07FFFFF...\n");

    uint32_t virt = 0xC0400000;
    uint32_t phys = 0x00500000;   /* physical memory after kernel + identity */

    /* 4 MB = 1024 pages x 4 KB */
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

    /* ---- Test 7: paging_create / free_directory (EXT-SW-01) ---- */
    serial_puts("[TEST 7] paging_create/free_directory (EXT-SW-01)...\n");
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

/* ------------------------------------------------------------------ */
/* Sprint-3 verification tests                              (S3)       */
/* ------------------------------------------------------------------ */
static void test_s3(void)
{
    serial_puts("\n==============================\n");
    serial_puts("  S3 VERIFICATION TESTS\n");
    serial_puts("==============================\n");

    /* ---- Test 1: divide-by-zero → ISR 0 fires ----
     *
     * DISABLED by default. To test:
     *   1. Uncomment the three lines below.
     *   2. Run QEMU and watch serial output.
     *   3. You should see: [ISR] Exception 0x0 (Divide-by-Zero)
     *      followed by "KERNEL PANIC: Divide-by-Zero" and a halt.
     *   4. Re-comment before continuing to S4.
     */
    /*
    serial_puts("[TEST S3-1] Triggering divide-by-zero (ISR 0)...\n");
    volatile int x = 1, y = 0;
    volatile int z = x / y;
    (void)z;
    */

    /* ---- Test 2: tick counter — watch PIT firing at 100 Hz ---- */
    serial_puts("[TEST S3-2] Tick counter check (busy-wait ~1 second)...\n");

    /*
     * Spin for approximately 1 second worth of ticks.
     * We cannot use a real sleep yet (no scheduler), so we watch the
     * tick counter increment.  At 100 Hz, 100 ticks = 1 second.
     *
     * This busy-wait relies on the fact that:
     *   - STI has already been called
     *   - IRQ0 fires every 10 ms and increments pit_tick_count
     *   - HLT releases the CPU until the next interrupt
     */
    uint32_t start = pit_get_ticks();
    uint32_t target = start + 100;          /* wait for 100 ticks = ~1 s */

    while (pit_get_ticks() < target) {
        __asm__ volatile ("hlt");           /* sleep until next IRQ */
    }

    uint32_t elapsed = pit_get_ticks() - start;
    serial_printf("  Ticks elapsed: %d (expected ~100)\n", elapsed);

    if (elapsed >= 95 && elapsed <= 105) {
        serial_puts("  PASS: PIT firing at ~100Hz\n");
    } else {
        serial_puts("  WARN: tick count outside expected range — "
                    "check PIT divisor or IRQ0 handler\n");
    }

    /* ---- Test 3: 10-second stability check (NFR-PERF-02) ----
     *
     * SRS NFR-PERF-02 requires ~1000 ticks over 10 seconds.
     * DISABLED by default because it adds 10 s to boot time.
     * Uncomment for final NFR validation in S5.
     */
    /*
    serial_puts("[TEST S3-3] 10s stability check (NFR-PERF-02)...\n");
    uint32_t t0 = pit_get_ticks();
    uint32_t t1 = t0 + 1000;
    while (pit_get_ticks() < t1) {
        __asm__ volatile ("hlt");
    }
    uint32_t got = pit_get_ticks() - t0;
    serial_printf("  Ticks in 10s: %d (expected ~1000)\n", got);
    if (got >= 950 && got <= 1050)
        serial_puts("  PASS: NFR-PERF-02 satisfied\n");
    else
        serial_puts("  FAIL: NFR-PERF-02 out of range\n");
    */

    /* ---- Test 4: GPF handler (ISR 13) ----
     *
     * DISABLED by default — triggers a deliberate GPF.
     * Uncomment to verify ISR 13 logs error_code + eip then halts.
     * You should see: [ISR] #GP error_code=... eip=...
     */
    // FIXED — use // comments inside the asm block:
    // 
    // __asm__ volatile (
    //     "mov $0x0, %ax \n"
    //     "mov %ax, %ds  \n"   // load null selector → #GP
    // );
    // 
    serial_puts("==============================\n");
    serial_puts("  S3 TESTS COMPLETE\n");
    serial_puts("==============================\n\n");
}

/* ================================================================== */
/*  kernel_main                                                        */
/* ================================================================== */
void kernel_main(uint32_t magic, uint32_t mb_info)
{
    /* ── S0: early I/O ──────────────────────────────────────────── */
    serial_init();
    serial_puts("[BOOT] Serial logger initialized\n");

    vga_init();
    serial_puts("[BOOT] VGA driver initialized\n");

    if (magic != MULTIBOOT_MAGIC) {
        vga_set_color(VGA_COLOR_RED, VGA_COLOR_BLACK);
        vga_puts("ERROR: Invalid Multiboot magic number!\n");
        serial_puts("[ERROR] Invalid Multiboot magic number!\n");
        goto hang;
    }

    serial_puts("\n[KERNEL] MiniOS — Sprint 1 + Sprint 2 + Sprint 3\n");
    serial_printf("[KERNEL] Multiboot magic: 0x%x\n", magic);

    /* ── S1: GDT ────────────────────────────────────────────────── */
    serial_puts("[KERNEL] Initialising GDT...\n");
    gdt_init();
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("[OK] GDT loaded (null + kernel code + kernel data)\n");
    serial_puts("[S1]  GDT initialized\n");

    /* ── S1: Physical memory allocator ─────────────────────────── */
    multiboot_info_t *mbi = (multiboot_info_t *)mb_info;
    serial_puts("[KERNEL] Initialising physical memory manager...\n");
    mm_init(mbi);
    vga_puts("[OK] Physical memory allocator ready\n");
    serial_puts("[S1]  mm_init() complete\n");

    /* ── S1: Frame self-test ────────────────────────────────────── */
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

    /* ── S2: Paging setup ───────────────────────────────────────── */
    serial_puts("[KERNEL] Setting up paging...\n");

    page_dir_t *kernel_dir = paging_create_directory();
    if (!kernel_dir) {
        serial_puts("[KERNEL] FATAL: could not allocate page directory\n");
        goto hang;
    }

    paging_identity_map_first4mb(kernel_dir);
    paging_map_kernel(kernel_dir);
    paging_map_heap(kernel_dir);
    paging_load_directory((uint32_t)kernel_dir);
    paging_enable();

    vga_puts("[OK] Paging enabled\n");
    serial_puts("[KERNEL] Paging ENABLED.\n");

    /* ── S2: Kernel heap ────────────────────────────────────────── */
    mm_heap_init();
    vga_puts("[OK] Kernel heap initialized\n");
    serial_puts("[S2]  mm_heap_init() complete\n");

    /* ── S3: Full IDT ───────────────────────────────────────────── */
    /*
     * S2's idt_load_minimal() + isr_handlers_init() are GONE.
     * S3 replaces them with a proper 256-entry IDT.
     * idt_init() wires all 32 exception stubs + 16 IRQ stubs and
     * calls LIDT.  Must happen before pic_remap() because the IDT
     * must be in place before we unmask IRQs.
     */
    idt_init();
    vga_puts("[OK] IDT loaded (256 gates)\n");
    serial_puts("[S3]  IDT loaded (256 gates)\n");

    /* ── S3: PIC remap ──────────────────────────────────────────── */
    /*
     * Without this, IRQ0 fires on vector 8 (#DF Double Fault) and
     * IRQ1 on vector 9, etc.  Must be done BEFORE STI.
     */
    pic_remap();
    vga_puts("[OK] PIC remapped (master=0x20, slave=0x28)\n");
    serial_puts("[S3]  PIC remapped: master=0x20 slave=0x28\n");

    /* ── S3: PIT ────────────────────────────────────────────────── */
    pit_init();
    vga_puts("[OK] PIT configured at 100Hz\n");
    serial_puts("[S3]  PIT configured at 100Hz (divisor=11932)\n");

    /* ── S3: ISR install + enable interrupts ────────────────────── */
    isr_install();

    __asm__ volatile ("sti");
    vga_puts("[OK] Interrupts enabled (STI)\n");
    serial_puts("[S3]  STI — interrupts enabled\n");

    /* ── Boot banner ────────────────────────────────────────────── */
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("=======================================\n");
    vga_puts("       MiniOS Kernel v0.3              \n");
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
    vga_puts("[OK] IDT loaded (256 gates)\n");
    vga_puts("[OK] PIC remapped, PIT at 100Hz\n");
    vga_puts("[OK] Interrupts enabled\n");
    vga_puts("\nSprint 1 + Sprint 2 + Sprint 3 complete. Ready for S4.\n");

    /* ── Run verification tests ─────────────────────────────────── */
    test_s2();
    test_s3();

    /* ── Hand off to S4 (Raunak) ────────────────────────────────── */
    serial_puts("[KERNEL] S3 complete. Ready for S4 (TCB + scheduler).\n");

hang:
    while (1) {
        __asm__ volatile ("hlt");
    }
}