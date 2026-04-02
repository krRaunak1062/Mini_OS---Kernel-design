/*
 * kernel/kernel_main.c
 * Main kernel entry point
 * Author: Aman Yadav
 */

#include <stdint.h>
#include "vga.h"
#include "serial.h"
#include "gdt.h"          /* S1: REQ-MEM-01 */
#include "mm_phys.h"      /* S1: REQ-MEM-03 */
#include "multiboot.h"    /* S1: Multiboot info struct */

/* Multiboot magic number that GRUB puts in EAX */
#define MULTIBOOT_MAGIC 0x2BADB002

/*
 * Function: kernel_main
 * Description: Main kernel entry point, called from boot.asm
 *
 * IMPORTANT — parameter order matches what boot.asm pushes:
 *   boot.asm does:  push ebx  (mb_info pointer, second push)
 *                   push eax  (magic, first push)
 *   So C sees:      kernel_main(magic, mb_info)   ← correct order
 *
 * For S1 we cast mb_info to multiboot_info_t* so mm_init() can
 * walk the memory map. It's still the same physical address GRUB
 * gave us — we just tell the compiler what it points to.
 *
 * Implements: REQ-MEM-01, REQ-MEM-03
 * Author: Aman Yadav (S1), Shardul Diwate (S0)
 */
void kernel_main(uint32_t magic, uint32_t mb_info)
{
    /* ── S0: early I/O ─────────────────────────────────────────── */

    serial_init();
    serial_puts("[BOOT] Serial logger initialized\n");

    vga_init();
    serial_puts("[BOOT] VGA driver initialized\n");

    /* Verify multiboot magic before touching mb_info pointer */
    if (magic != MULTIBOOT_MAGIC) {
        vga_set_color(VGA_COLOR_RED, VGA_COLOR_BLACK);
        vga_puts("ERROR: Invalid Multiboot magic number!\n");
        serial_puts("[ERROR] Invalid Multiboot magic number!\n");
        goto hang;
    }

    /* ── S1: GDT ───────────────────────────────────────────────── */
    /*
     * Must be first subsystem call. CPU segment registers have
     * whatever GRUB left in them — undefined for our purposes.
     * gdt_init() loads our 3-entry flat GDT and flushes CS/DS/etc.
     */
    gdt_init();
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("[OK] GDT loaded (null + kernel code + kernel data)\n");
    serial_puts("[S1] GDT initialized\n");

    /* ── S1: Physical memory allocator ────────────────────────── */
    /*
     * Cast mb_info to multiboot_info_t* — this is the physical address
     * GRUB put in EBX. mm_init() walks mbi->mmap_addr to find usable
     * RAM and builds our bitmap. Must happen before any frame allocs.
     * Must happen before paging (S2) since the pointer is physical.
     */
    multiboot_info_t *mbi = (multiboot_info_t *)mb_info;
    mm_init(mbi);
    vga_puts("[OK] Physical memory allocator ready\n");
    serial_puts("[S1] mm_init() complete\n");

    /* ── S1: Self-test ─────────────────────────────────────────── */
    /*
     * Alloc and free 3 frames as a quick sanity check.
     * Remove this block once S2 starts — it's just for GDB verification.
     */
    uint32_t f1 = mm_alloc_frame();
    uint32_t f2 = mm_alloc_frame();
    uint32_t f3 = mm_alloc_frame();
    serial_puts("[S1] test alloc: ");
    serial_puts_hex(f1); serial_puts(" ");
    serial_puts_hex(f2); serial_puts(" ");
    serial_puts_hex(f3); serial_puts("\n");
    mm_free_frame(f1);
    mm_free_frame(f2);
    mm_free_frame(f3);
    serial_puts("[S1] test free: OK\n");

    /* ── Boot banner ───────────────────────────────────────────── */
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("=======================================\n");
    vga_puts("       MiniOS Kernel v0.1              \n");
    vga_puts("       Group 31 | IIT Jodhpur          \n");
    vga_puts("=======================================\n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("[OK] VGA driver active\n");
    vga_puts("[OK] Serial logger active (COM1 9600 baud)\n");
    vga_puts("[OK] Protected mode confirmed\n");
    vga_puts("[OK] GDT initialized\n");
    vga_puts("[OK] Physical allocator ready\n");
    vga_puts("\nSprint 1 complete. Ready for S2.\n");

    serial_puts("[BOOT] Sprint 1 complete. Ready for S2.\n");

hang:
    while (1) {
        __asm__ volatile ("hlt");
    }
}