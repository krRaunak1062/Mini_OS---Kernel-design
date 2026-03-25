/*
 * kernel/vga.c
 * VGA text mode driver
 * Implements: EXT-UI-01
 * Author: Shardul Diwate
 */

#include "vga.h"

/* VGA constants */
#define VGA_WIDTH    80
#define VGA_HEIGHT   25
#define VGA_MEMORY   ((uint16_t *)0xB8000)

/* Current cursor position and color */
static uint16_t cursor_row = 0;
static uint16_t cursor_col = 0;
static uint8_t  current_color;

/* Helper: combine fg and bg into VGA attribute byte */
static uint8_t vga_make_color(vga_color_t fg, vga_color_t bg)
{
    return fg | (bg << 4);
}

/* Helper: combine character and color into VGA entry */
static uint16_t vga_make_entry(char c, uint8_t color)
{
    return (uint16_t)c | ((uint16_t)color << 8);
}

/*
 * Function: vga_init
 * Description: Initializes VGA driver with default colors and clears screen
 * Implements: EXT-UI-01
 * Input: None
 * Output: None
 * Author: Shardul Diwate
 */
void vga_init(void)
{
    current_color = vga_make_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    vga_clear();
}

/*
 * Function: vga_clear
 * Description: Clears entire VGA screen with current color
 * Implements: EXT-UI-01
 * Input: None
 * Output: None
 * Author: Shardul Diwate
 */
void vga_clear(void)
{
    for (uint16_t row = 0; row < VGA_HEIGHT; row++) {
        for (uint16_t col = 0; col < VGA_WIDTH; col++) {
            VGA_MEMORY[row * VGA_WIDTH + col] =
                vga_make_entry(' ', current_color);
        }
    }
    cursor_row = 0;
    cursor_col = 0;
}

/*
 * Function: vga_set_color
 * Description: Sets foreground and background color for future output
 * Implements: EXT-UI-01
 * Input: fg - foreground color, bg - background color
 * Output: None
 * Author: Shardul Diwate
 */
void vga_set_color(vga_color_t fg, vga_color_t bg)
{
    current_color = vga_make_color(fg, bg);
}

/* Helper: scroll screen up one line when bottom is reached */
static void vga_scroll(void)
{
    /* Move every row up by one */
    for (uint16_t row = 0; row < VGA_HEIGHT - 1; row++) {
        for (uint16_t col = 0; col < VGA_WIDTH; col++) {
            VGA_MEMORY[row * VGA_WIDTH + col] =
                VGA_MEMORY[(row + 1) * VGA_WIDTH + col];
        }
    }
    /* Clear the last row */
    for (uint16_t col = 0; col < VGA_WIDTH; col++) {
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + col] =
            vga_make_entry(' ', current_color);
    }
    cursor_row = VGA_HEIGHT - 1;
}

/*
 * Function: vga_putchar
 * Description: Prints a single character to VGA console
 * Implements: EXT-UI-01
 * Input: c - character to print
 * Output: None
 * Author: Shardul Diwate
 */
void vga_putchar(char c)
{
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\r') {
        cursor_col = 0;
    } else if (c == '\t') {
        cursor_col = (cursor_col + 8) & ~7;
    } else {
        VGA_MEMORY[cursor_row * VGA_WIDTH + cursor_col] =
            vga_make_entry(c, current_color);
        cursor_col++;
    }

    /* Wrap to next line if past screen width */
    if (cursor_col >= VGA_WIDTH) {
        cursor_col = 0;
        cursor_row++;
    }

    /* Scroll if past screen height */
    if (cursor_row >= VGA_HEIGHT) {
        vga_scroll();
    }
}

/*
 * Function: vga_puts
 * Description: Prints a null-terminated string to VGA console
 * Implements: EXT-UI-01
 * Input: str - string to print
 * Output: None
 * Author: Shardul Diwate
 */
void vga_puts(const char *str)
{
    while (*str) {
        vga_putchar(*str++);
    }
}