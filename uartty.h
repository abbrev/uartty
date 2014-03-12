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

#ifndef UARTTY_LOG2_TX_BUF_SIZE
#define UARTTY_LOG2_TX_BUF_SIZE 4
#endif

#ifndef UARTTY_LOG2_RX_BUF_SIZE
#define UARTTY_LOG2_RX_BUF_SIZE 8
#endif

#if UARTTY_LOG2_TX_BUF_SIZE < 3 || UARTTY_LOG2_TX_BUF_SIZE > 8
#error UARTTY_LOG2_TX_BUF_SIZE must be between 3 and 8!
#endif

#if UARTTY_LOG2_RX_BUF_SIZE < 0 || UARTTY_LOG2_RX_BUF_SIZE > 8
#error UARTTY_LOG2_RX_BUF_SIZE must be between 0 and 8!
#endif

#endif
