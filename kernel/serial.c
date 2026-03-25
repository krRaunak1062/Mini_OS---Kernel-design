/*
 * kernel/serial.c
 * COM1 serial port debug logger
 * Implements: EXT-COM-01
 * Author: Shardul Diwate
 */

#include "serial.h"

/*
 * Helper: write a byte to an I/O port
 * All port I/O must stay in arch/x86/ per NFR-PORT-01
 * Using inline here only for serial driver simplicity in S0
 */
static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

/*
 * Helper: read a byte from an I/O port
 */
static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

/*
 * Function: serial_init
 * Description: Initializes COM1 serial port at 9600 baud
 * Implements: EXT-COM-01
 * Input: None
 * Output: None
 * Author: Shardul Diwate
 */
void serial_init(void)
{
    outb(COM1 + 1, 0x00);  /* Disable all interrupts */
    outb(COM1 + 3, 0x80);  /* Enable DLAB to set baud rate */
    outb(COM1 + 0, 0x0C);  /* Baud rate divisor low byte  (9600 baud) */
    outb(COM1 + 1, 0x00);  /* Baud rate divisor high byte (9600 baud) */
    outb(COM1 + 3, 0x03);  /* 8 bits, no parity, one stop bit */
    outb(COM1 + 2, 0xC7);  /* Enable FIFO, clear, 14-byte threshold */
    outb(COM1 + 4, 0x0B);  /* IRQs enabled, RTS/DSR set */
}

/*
 * Helper: wait until serial port is ready to transmit
 */
static int serial_is_transmit_empty(void)
{
    return inb(COM1 + 5) & 0x20;
}

/*
 * Function: serial_putchar
 * Description: Sends a single character over COM1 serial port
 * Implements: EXT-COM-01
 * Input: c - character to send
 * Output: None
 * Author: Shardul Diwate
 */
void serial_putchar(char c)
{
    /* Wait until transmit buffer is empty */
    while (serial_is_transmit_empty() == 0);
    outb(COM1, c);
}

/*
 * Function: serial_puts
 * Description: Sends a null-terminated string over COM1 serial port
 * Implements: EXT-COM-01
 * Input: str - string to send
 * Output: None
 * Author: Shardul Diwate
 */
void serial_puts(const char *str)
{
    while (*str) {
        /* Convert \n to \r\n for serial terminals */
        if (*str == '\n') {
            serial_putchar('\r');
        }
        serial_putchar(*str++);
    }
}