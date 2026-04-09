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

void serial_puts_hex(uint32_t val)
{
    char buf[11];   /* "0x" + 8 hex digits + null */
    buf[0] = '0'; buf[1] = 'x';
    for (int i = 9; i >= 2; i--) {
        uint8_t nibble = val & 0xF;
        buf[i] = nibble < 10 ? '0' + nibble : 'A' + nibble - 10;
        val >>= 4;
    }
    buf[10] = '\0';
    serial_puts(buf);
}

/*
 * Function: serial_log
 * Description: printf-style serial logger using variadic args.
 * Supports: %s (string), %d (decimal), %u (unsigned), %x (hex), %c (char)
 *
 * NOTE: serial.h defines  #define serial_printf serial_log
 *       so all serial_printf() call sites resolve here at compile time.
 *       Do NOT add a separate serial_printf() function — it would clash
 *       with that macro and cause a redefinition error.
 *
 * BUG FIX (S2): %x no longer emits a hardcoded "0x" prefix.
 *   The specifier now prints ONLY the hex digits, matching standard
 *   printf behaviour.  Callers that want the "0x" prefix must write it
 *   in the format string:  serial_printf("addr: 0x%x\n", val)
 *   This eliminates the "0x0x..." double-prefix seen in S2 serial output.
 *
 * Implements: EXT-COM-01
 */
void serial_log(const char *fmt, ...)
{
    __builtin_va_list args;
    __builtin_va_start(args, fmt);

    while (*fmt) {
        if (*fmt != '%') {
            serial_putchar(*fmt++);
            continue;
        }
        fmt++;  /* skip % */
        switch (*fmt) {
            case 's': {
                const char *s = __builtin_va_arg(args, const char *);
                if (s) serial_puts(s);
                break;
            }
            case 'd': {
                int val = __builtin_va_arg(args, int);
                if (val < 0) { serial_putchar('-'); val = -val; }
                char buf[12]; int i = 0;
                if (val == 0) { serial_putchar('0'); break; }
                while (val > 0) { buf[i++] = '0' + (val % 10); val /= 10; }
                while (i--) serial_putchar(buf[i]);
                break;
            }
            case 'u': {
                /* Unsigned decimal — needed for sizes/counts that may
                 * exceed INT_MAX (e.g. heap total_free bytes). */
                uint32_t val = __builtin_va_arg(args, uint32_t);
                char buf[12]; int i = 0;
                if (val == 0) { serial_putchar('0'); break; }
                while (val > 0) { buf[i++] = '0' + (val % 10); val /= 10; }
                while (i--) serial_putchar(buf[i]);
                break;
            }
            case 'x': {
                /* Prints raw hex digits only — NO "0x" prefix added here.
                 * Write "0x%x" in the format string if the prefix is wanted. */
                uint32_t val = __builtin_va_arg(args, uint32_t);
                for (int i = 28; i >= 0; i -= 4) {
                    uint8_t nibble = (val >> i) & 0xF;
                    serial_putchar(nibble < 10 ? '0'+nibble : 'A'+nibble-10);
                }
                break;
            }
            case 'c': {
                char c = (char)__builtin_va_arg(args, int);
                serial_putchar(c);
                break;
            }
            default:
                serial_putchar('%');
                serial_putchar(*fmt);
                break;
        }
        fmt++;
    }
    __builtin_va_end(args);
}