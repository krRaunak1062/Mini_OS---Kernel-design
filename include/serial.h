#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

/* COM1 base port address */
#define COM1 0x3F8

/* Function prototypes */
void serial_init(void);
void serial_putchar(char c);
void serial_puts(const char *str);
void serial_puts_hex(uint32_t val);
void serial_log(const char *fmt, ...);

/* S2 alias — serial_printf maps to serial_log (same function) */
#define serial_printf serial_log

#endif /* SERIAL_H */