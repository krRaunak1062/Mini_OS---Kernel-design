/*
 * kernel/kernel_main.c
 * Main kernel entry point — Sprint 1 + Sprint 2 + Sprint 3 + Sprint 4.
 *
 * Implements:
 *   S1 : REQ-MEM-01 (GDT), REQ-MEM-03 (physical allocator)
 *   S2 : REQ-MEM-02, REQ-MEM-04, REQ-MEM-05 (paging + heap)
 *   S3 : REQ-TASK-01 (IDT), REQ-TASK-02 (PIC + PIT + IRQ handlers)
 *   S4 : REQ-TASK-03 (TCB), REQ-TASK-04 (context switch),
 *        REQ-TASK-05 (round-robin scheduler)
 *
 * Authors:
 *   Shardul Diwate        (B24CS1028) — boot scaffolding (S0), build (S6)
 *   Aman Yadav            (B24CS1006) — memory subsystem calls (S1, S2)
 *   Raunak Kumar          (B24CS1062) — interrupt infrastructure (S3), scheduler (S4)
 *   Palthyavath Jalendhar (B24CS1051) — integration (S5)
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
 *  10.  paging_enable()                 S2 : set CR0.PG  <- PAGING ON
 *  11.  mm_heap_init()                  S2 : initialise heap free list
 *  12.  idt_init()                      S3 : build + load 256-entry IDT
 *  13.  pic_remap()                     S3 : remap IRQs to 0x20-0x2F
 *  14.  pit_init()                      S3 : PIT channel 0 at 100 Hz
 *  15.  isr_install()                   S3 : confirm C handlers active
 *  16.  sched_init()                    S4 : initialise ready queue
 *  17.  task_create() x3               S4 : allocate TCBs + kernel stacks
 *  18.  sched_add_task() x3            S4 : enqueue tasks
 *  19.  STI + sched_start()            S4 : enable IRQs, IRET into first task
 *       (sched_start never returns — IRQ0 drives all further switches)
 */

#include <stdint.h>
#include "vga.h"
#include "serial.h"
#include "gdt.h"
#include "mm_phys.h"
#include "multiboot.h"
#include "mm_heap.h"
#include "paging.h"
#include "idt.h"
#include "isr.h"
#include "pic.h"
#include "pit.h"
#include "task.h"
#include "sched.h"

#define MULTIBOOT_MAGIC 0x2BADB002

/* ------------------------------------------------------------------ */
/* Heap mapping helper                                      (S2)       */
/* ------------------------------------------------------------------ */
static void paging_map_heap(page_dir_t *dir)
{
    serial_puts("[KERNEL] Mapping heap region 0xC0400000-0xC07FFFFF...\n");

    uint32_t virt = 0xC0400000;
    uint32_t phys = 0x00500000;

    for (uint32_t i = 0; i < 1024; i++) {
        paging_map_page(dir, virt, phys, PAGE_RW);
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

    serial_puts("[TEST 1] Paging active check...\n");
    uint32_t cr0;
    __asm__ __volatile__("mov %%cr0, %0" : "=r"(cr0));
    if (cr0 & 0x80000000)
        serial_puts("  PASS: CR0.PG is set\n");
    else
        serial_puts("  FAIL: CR0.PG is NOT set\n");

    serial_printf("[TEST 2] CR3 = 0x%x\n", paging_get_cr3());

    serial_puts("[TEST 3] kmalloc basic...\n");
    void *p1 = kmalloc(64);
    void *p2 = kmalloc(128);
    void *p3 = kmalloc(256);
    if (p1 && p2 && p3)
        serial_printf("  PASS: p1=0x%x p2=0x%x p3=0x%x\n",
                      (uint32_t)p1, (uint32_t)p2, (uint32_t)p3);
    else
        serial_puts("  FAIL: kmalloc returned NULL\n");

    serial_puts("[TEST 4] kfree and realloc...\n");
    kfree(p2);
    void *p4 = kmalloc(128);
    if (p4 == p2)
        serial_puts("  PASS: freed block was reused\n");
    else
        serial_printf("  INFO: p4=0x%x (different from p2=0x%x)\n",
                      (uint32_t)p4, (uint32_t)p2);
    kfree(p1); kfree(p3); kfree(p4);

    serial_puts("[TEST 5] Stress: 100 allocs of 64 bytes...\n");
    void *ptrs[100];
    int ok = 1;
    for (int i = 0; i < 100; i++) {
        ptrs[i] = kmalloc(64);
        if (!ptrs[i]) { ok = 0; break; }
    }
    for (int i = 0; i < 100; i++)
        if (ptrs[i]) kfree(ptrs[i]);
    serial_printf("  %s: 100 alloc/free cycle\n", ok ? "PASS" : "FAIL");

    mm_heap_dump();

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

    serial_puts("[TEST S3-2] Tick counter check (busy-wait ~1 second)...\n");
    uint32_t start  = pit_get_ticks();
    uint32_t target = start + 100;
    while (pit_get_ticks() < target)
        __asm__ volatile ("hlt");
    uint32_t elapsed = pit_get_ticks() - start;
    serial_printf("  Ticks elapsed: %d (expected ~100)\n", elapsed);
    if (elapsed >= 95 && elapsed <= 105)
        serial_puts("  PASS: PIT firing at ~100Hz\n");
    else
        serial_puts("  WARN: tick count outside expected range\n");

    serial_puts("==============================\n");
    serial_puts("  S3 TESTS COMPLETE\n");
    serial_puts("==============================\n\n");
}

/* ------------------------------------------------------------------ */
/* Sprint-4 demo task functions                             (S4)       */
/*                                                                     */
/* Each task busy-waits ~5 million iterations between prints.         */
/* This keeps QEMU CPU usage low and prevents host system slowdown.   */
/*                                                                     */
/* The volatile keyword on the counter prevents the compiler from     */
/* optimising the delay loop away entirely.                           */
/*                                                                     */
/* Implements: REQ-TASK-05                                            */
/* ------------------------------------------------------------------ */
static void task_a(void)
{
    while (1) {
        serial_puts("[TASK A] PID=1 running\n");
        for (volatile int i = 0; i < 5000000; i++);
    }
}

static void task_b(void)
{
    while (1) {
        serial_puts("[TASK B] PID=2 running\n");
        for (volatile int i = 0; i < 5000000; i++);
    }
}

static void task_c(void)
{
    while (1) {
        serial_puts("[TASK C] PID=3 running\n");
        for (volatile int i = 0; i < 5000000; i++);
    }
}

/* ================================================================== */
/*  kernel_main                                                        */
/* ================================================================== */
void kernel_main(uint32_t magic, uint32_t mb_info)
{
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

    serial_puts("\n[KERNEL] MiniOS — Sprint 1 + Sprint 2 + Sprint 3 + Sprint 4\n");
    serial_printf("[KERNEL] Multiboot magic: 0x%x\n", magic);

    serial_puts("[KERNEL] Initialising GDT...\n");
    gdt_init();
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("[OK] GDT loaded (null + kernel code + kernel data)\n");
    serial_puts("[S1]  GDT initialized\n");

    multiboot_info_t *mbi = (multiboot_info_t *)mb_info;
    serial_puts("[KERNEL] Initialising physical memory manager...\n");
    mm_init(mbi);
    vga_puts("[OK] Physical memory allocator ready\n");
    serial_puts("[S1]  mm_init() complete\n");

    uint32_t f1 = mm_alloc_frame();
    uint32_t f2 = mm_alloc_frame();
    uint32_t f3 = mm_alloc_frame();
    serial_puts("[S1]  test alloc: ");
    serial_puts_hex(f1); serial_puts(" ");
    serial_puts_hex(f2); serial_puts(" ");
    serial_puts_hex(f3); serial_puts("\n");
    mm_free_frame(f1); mm_free_frame(f2); mm_free_frame(f3);
    serial_puts("[S1]  test free: OK\n");

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

    mm_heap_init();
    vga_puts("[OK] Kernel heap initialized\n");
    serial_puts("[S2]  mm_heap_init() complete\n");

    idt_init();
    vga_puts("[OK] IDT loaded (256 gates)\n");
    serial_puts("[S3]  IDT loaded (256 gates)\n");

    pic_remap();
    vga_puts("[OK] PIC remapped (master=0x20, slave=0x28)\n");
    serial_puts("[S3]  PIC remapped: master=0x20 slave=0x28\n");

    pit_init();
    vga_puts("[OK] PIT configured at 100Hz\n");
    serial_puts("[S3]  PIT configured at 100Hz (divisor=11932)\n");

    isr_install();
    serial_puts("[S3]  ISR handlers installed\n");

    __asm__ volatile ("sti");
    test_s2();
    test_s3();
    __asm__ volatile ("cli");

    sched_init();
    vga_puts("[OK] Scheduler initialized\n");
    serial_puts("[S4]  sched_init() complete\n");

    tcb_t *ta = task_create(task_a);
    tcb_t *tb = task_create(task_b);
    tcb_t *tc = task_create(task_c);

    if (!ta || !tb || !tc) {
        serial_puts("[KERNEL] FATAL: task_create() returned NULL\n");
        goto hang;
    }

    serial_printf("[S4]  Task A created: PID=%d kernel_esp=0x%x\n",
                  ta->pid, ta->kernel_esp);
    serial_printf("[S4]  Task B created: PID=%d kernel_esp=0x%x\n",
                  tb->pid, tb->kernel_esp);
    serial_printf("[S4]  Task C created: PID=%d kernel_esp=0x%x\n",
                  tc->pid, tc->kernel_esp);

    sched_add_task(ta);
    sched_add_task(tb);
    sched_add_task(tc);
    serial_puts("[S4]  Three tasks enqueued in ready queue\n");

    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("=======================================\n");
    vga_puts("       MiniOS Kernel v0.4              \n");
    vga_puts("       Group 31 | IIT Jodhpur          \n");
    vga_puts("=======================================\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("[OK] Scheduler ready, 3 tasks queued\n");
    vga_puts("\nSprint 1-4 complete. Starting scheduler...\n");

    serial_puts("[KERNEL] Sprints 1-4 complete.\n");
    serial_puts("[S4]  Enabling interrupts and starting round-robin scheduler.\n");

    __asm__ volatile ("sti");
    sched_start();

    serial_puts("[KERNEL] ERROR: sched_start() returned unexpectedly!\n");

hang:
    while (1) {
        __asm__ volatile ("hlt");
    }
}
