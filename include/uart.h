#ifndef _UART_H
#define _UART_H

#include "types.h"

#define UART0_BASE 0x09000000ULL

void uart_init(void);
void uart_putc(char c);
char uart_getc(void);
int  uart_getc_nonblock(char *c);
void uart_puts(const char *str);

#endif /* _UART_H */
