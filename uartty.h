/*
 * UARTTY, a simple UART + TTY for AVR.
 * This is a serial driver that also performs the following basic TTY
 * functions:
 * - line editing (backspace and ^U)
 * - translate NL to CRNL on output
 * - translate CR to NL on input
 */

#ifndef _UARTTY_H_
#define _UARTTY_H_

#include <stdio.h>

void uartty_init(unsigned int baudrate);
int uartty_getc(FILE *unused);
int uartty_getc_nb(FILE *unused);
int uartty_putc(char data, FILE *unused);
int uartty_putc_nb(char data, FILE *unused);
//void uartty_puts(const char *s);
//void uartty_puts_p(const char *s);

#endif
