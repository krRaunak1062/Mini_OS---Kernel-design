#ifndef SERIAL_H
#define SERIAL_H

#include <stdint.h>

/* COM1 base port address */
#define COM1 0x3F8

/* Function prototypes */
void serial_init(void);
void serial_putchar(char c);
void serial_puts(const char *str);

#endif /* SERIAL_H */