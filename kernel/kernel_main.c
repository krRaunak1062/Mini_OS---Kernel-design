/*
 * kernel/kernel_main.c
 * Main kernel entry point — Sprint 1–5.
 *
 * Implements:
 *   S1 : REQ-MEM-01 (GDT), REQ-MEM-03 (physical allocator)
 *   S2 : REQ-MEM-02, REQ-MEM-04, REQ-MEM-05 (paging + heap)
 *   S3 : REQ-TASK-01 (IDT), REQ-TASK-02 (PIC + PIT + IRQ handlers)
 *   S4 : REQ-TASK-03 (TCB), REQ-TASK-04 (context switch),
 *        REQ-TASK-05 (round-robin scheduler)
 *   S5 : NFR-PERF-01/02, NFR-SEC-01, NFR-REL-01, NFR-MAINT-01,
 *        NFR-PORT-01, EXT-SW-01 (integration & NFR validation)
 *
 * Authors:
 *   Shardul Diwate        (B24CS1028) — boot scaffolding (S0), build (S6)
 *   Aman Yadav            (B24CS1006) — memory subsystem calls (S1, S2)
 *   Raunak Kumar          (B24CS1062) — interrupt infrastructure (S3), scheduler (S4)
 *   Palthyavath Jalendhar (B24CS1051) — integration (S5)
 *
 * Call order (critical — DO NOT reorder):
 *   1.  serial_init()
 *   2.  vga_init()
 *   3.  gdt_init()
 *   4.  mm_init()
 *   5.  paging_create_directory()
 *   6.  paging_identity_map_first4mb()
 *   7.  paging_map_kernel()
 *   8.  paging_map_heap()
 *   9.  paging_load_directory()
 *  10.  paging_enable()                 <- PAGING ON
 *  11.  mm_heap_init()
 *  12.  idt_init()
 *  13.  pic_remap()
 *  14.  pit_init()
 *  15.  isr_install()
 *  16.  STI                             <- IRQs enabled
 *  17.  s5_run_all_tests()              <- S5 tests (NO tasks in queue yet)
 *  18.  CLI
 *  19.  sched_init()
 *  20.  task_create() x3
 *  21.  sched_add_task() x3
 *  22.  STI + sched_start()             <- never returns
 *
 * CRITICAL ORDER RATIONALE (S5):
 *   s5_run_all_tests() runs BEFORE sched_init() and BEFORE any tasks
 *   are enqueued.  This means sched_switch() is a no-op during the
 *   tests (current_task == NULL) even if IRQ0 fires during PERF-02's
 *   10-second wait.  The scheduler is only armed once all NFR tests
 *   have passed and we deliberately start it.
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
#include "s5_integration.h"

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
/* Sprint-4 demo task functions                             (S4)       */
/* Each task uses a short busy-wait so it prints within one quantum.  */
/* Implements: REQ-TASK-05                                            */
/* ------------------------------------------------------------------ */
static void task_a(void)
{
    while (1) {
        serial_puts("[TASK A] PID=1 running\n");
        for (volatile int i = 0; i < 200000; i++);
    }
}

static void task_b(void)
{
    while (1) {
        serial_puts("[TASK B] PID=2 running\n");
        for (volatile int i = 0; i < 200000; i++);
    }
}

static void task_c(void)
{
    while (1) {
        serial_puts("[TASK C] PID=3 running\n");
        for (volatile int i = 0; i < 200000; i++);
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

    serial_puts("\n[KERNEL] MiniOS — Sprints 1-5\n");
    serial_printf("[KERNEL] Multiboot magic: 0x%x\n", magic);

    /* ---- S1 ---- */
    serial_puts("[KERNEL] Initialising GDT...\n");
    gdt_init();
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("[OK] GDT loaded (null + kernel code + kernel data)\n");
    serial_puts("[S1]  GDT initialized\n");

    multiboot_info_t *mbi = (multiboot_info_t *)mb_info;
    mm_init(mbi);
    vga_puts("[OK] Physical memory allocator ready\n");
    serial_puts("[S1]  mm_init() complete\n");

    /* ---- S2 ---- */
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

    /* ---- S3 ---- */
    idt_init();
    vga_puts("[OK] IDT loaded (256 gates)\n");
    serial_puts("[S3]  IDT loaded\n");

    pic_remap();
    vga_puts("[OK] PIC remapped (master=0x20, slave=0x28)\n");
    serial_puts("[S3]  PIC remapped\n");

    pit_init();
    vga_puts("[OK] PIT configured at 100Hz\n");
    serial_puts("[S3]  PIT configured at 100Hz\n");

    isr_install();
    serial_puts("[S3]  ISR handlers installed\n");

    /* ---- S5: run all NFR tests BEFORE the scheduler is armed ----
     *
     * Enable interrupts so PIT ticks fire (needed for PERF-02 timing).
     * current_task is still NULL so sched_switch() is a no-op when
     * IRQ0 fires during the tests — the scheduler cannot interfere.
     */
    __asm__ volatile ("sti");
    s5_run_all_tests();
    __asm__ volatile ("cli");

    s5_print_vga_summary();

    /* ---- S4: arm the scheduler ---- */
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

    serial_printf("[S4]  Task A: PID=%d kernel_esp=0x%x\n",
                  ta->pid, ta->kernel_esp);
    serial_printf("[S4]  Task B: PID=%d kernel_esp=0x%x\n",
                  tb->pid, tb->kernel_esp);
    serial_printf("[S4]  Task C: PID=%d kernel_esp=0x%x\n",
                  tc->pid, tc->kernel_esp);

    sched_add_task(ta);
    sched_add_task(tb);
    sched_add_task(tc);
    serial_puts("[S4]  Three tasks enqueued\n");

    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("=======================================\n");
    vga_puts("       MiniOS Kernel v0.5              \n");
    vga_puts("       Group 31 | IIT Jodhpur          \n");
    vga_puts("       Sprints 1-5 Complete            \n");
    vga_puts("=======================================\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("Starting scheduler...\n");

    serial_puts("[KERNEL] Sprints 1-5 complete. Starting scheduler.\n");

    __asm__ volatile ("sti");
    sched_start();                   /* never returns */

    serial_puts("[KERNEL] ERROR: sched_start() returned!\n");

hang:
    while (1) __asm__ volatile ("hlt");
}