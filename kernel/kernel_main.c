/*
 * kernel/kernel_main.c
 * Main kernel entry point
 * Author: Aman Yadav
 */

#include "vga.h"
#include "serial.h"

/* Multiboot magic number */
#define MULTIBOOT_MAGIC 0x2BADB002

/*
 * Function: kernel_main
 * Description: Main kernel entry point, called from boot.asm
 * Input: magic - multiboot magic number
 *        mb_info - pointer to multiboot info structure
 * Output: None
 * Author: Shardul Diwate
 */
void kernel_main(uint32_t magic, uint32_t mb_info)
{
    /* Initialize serial port first for early debugging */
    serial_init();
    serial_puts("[BOOT] Serial logger initialized\n");

    /* Initialize VGA text driver */
    vga_init();
    serial_puts("[BOOT] VGA driver initialized\n");

    /* Verify multiboot magic number */
    if (magic != MULTIBOOT_MAGIC) {
        vga_set_color(VGA_COLOR_RED, VGA_COLOR_BLACK);
        vga_puts("ERROR: Invalid Multiboot magic number!\n");
        serial_puts("[ERROR] Invalid Multiboot magic number!\n");
        goto hang;
    }

    /* Print boot banner to VGA */
    vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    vga_puts("=======================================\n");
    vga_puts("       MiniOS Kernel v0.1              \n");
    vga_puts("       Group 31 | IIT Jodhpur          \n");
    vga_puts("=======================================\n");

    /* Print in white for normal messages */
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_puts("\n[OK] Kernel loaded successfully\n");
    vga_puts("[OK] VGA driver active\n");
    vga_puts("[OK] Serial logger active (COM1 9600 baud)\n");
    vga_puts("[OK] Protected mode confirmed\n");
    vga_puts("\nSprint 0 complete. Ready for S1.\n");

    /* Mirror everything to serial */
    serial_puts("[OK] Kernel loaded successfully\n");
    serial_puts("[OK] VGA driver active\n");
    serial_puts("[OK] Serial logger active\n");
    serial_puts("[OK] Protected mode confirmed\n");
    serial_puts("[BOOT] Sprint 0 complete. Ready for S1.\n");

hang:
    /* Hang forever — kernel has nothing else to do yet */
    while (1) {
        __asm__ volatile ("hlt");
    }
}