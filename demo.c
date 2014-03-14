#include <stdio.h>

#include "uartty.h"

#define BAUD 9600
#define UBRR_VALUE (unsigned)((F_CPU) / (16.0 * (BAUD)) + 0.5 - 1)

static FILE uartty_file = FDEV_SETUP_STREAM(uartty_putc, uartty_getc,
		_FDEV_SETUP_RW);

int main(void)
{
	uartty_init(UBRR_VALUE);
	stdin = &uartty_file;
	stdout = &uartty_file;
	printf("Hello, world!\n");

	return 0;
}
