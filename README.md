# UARTTY

## Introduction

UARTTY is a UART driver for the AVR ATMEGA328P with built-in support for
basic TTY features, such as line editing.

## Features

With UARTTY you can treat your AVR like a tiny POSIX system!

Anything you send to UARTTY is echoed back to the terminal, just as you
might expect.

The most common line-editing characters are supported:
* Backspace erases the last character
* Ctrl-W erases the last word
* Ctrl-U erases the whole line
* Enter sends the line to the application

Note: other editor keys, such as Home/End, Delete, and the arrow keys,
are not supported. Signal characters such as Ctrl-C and Ctrl-Z are also
not supported; those would require POSIX signal support on the AVR.

Some conversions are made automatically when characters are sent or
received:
* Carriage return from the terminal is converted to a linefeed
* Linefeed from the AVR is converted to carriage return plus linefeed

This behavior can be customized at compile time through a set of
options (see [Compile-time options](#compile-time-options)).

## Code Example

(This code example is taken from the avr-libc manual and modified for
UARTTY.)

```C
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

	for (;;) {
		putchar(getchar());
	}
}
```

## Motivation

I needed a UART driver with basic line-editing capabilities for an AVR
project that I'm working on. I decided to write a new driver after the
POSIX TTY model as that is perhaps the most widely-used standard for
line-oriented terminal systems, and it provides all of the functionality
that I would need (erase characters, erase line, translate carriage
returns/linefeeds, etc.).

The TTY processing occurs asynchronously during serial send and receive
operations, and it's not too much to ask the UART transmit and receive
interrupts to perform a bit of extra processing on top of the more usual
task of adding the received byte to a queue or removing a byte from a
queue.

## Installation

Get the latest version of this library with this command:

    git clone https://github.com/abbrev/uartty.git

## API Reference

This API is designed to be usable with the AVR-libc stdio module:

```C
void uartty_init(unsigned int ubrr);
```

Initialize the UARTTY library. Pass in the UBRR value.

```C
int uartty_getc(FILE *unused);
```

Read a character from the UART.

```C
int uartty_getc_nb(FILE *unused);
```

Read a character from the UART without blocking.

```C
int uartty_putc(char data, FILE *unused);
```

Write a character to the UART.

```C
int uartty_putc_nb(char data, FILE *unused);
```

Write a character to the UART without blocking.

## <a name="compile-time-options"></a>Compile-time options

UARTTY can be configured at compile-time to behave differently. The
default settings work well in most situations. All of the following
options can be modified in the uartty-config.h file.

Set UARTTY\_PRESET to one of these symbols to enable/disable a variety
of modes:
* UARTTY\_PRESET\_RAW: no echo or input/output processing; a basic UART
* UARTTY\_PRESET\_SANE: echo, canonical input processing, etc.

If you know what you're doing, you can disable all presets and instead
enable or disable each option individually:
* UARTTY\_ICANON: enable canonical processing
* UARTTY\_IEXTEN: enable extended input processing
* UARTTY\_ECHO: echo received characters
* UARTTY\_ECHOE: echo character erases
* UARTTY\_ECHOK: kill character erases the current line
* UARTTY\_ECHONL: echo newline even when ECHO is not set
* UARTTY\_ECHOCTL: echo control characters as ^X
* UARTTY\_ERASE: character to erase the previous character
* UARTTY\_WERASE: characte to erase the previous word
* UARTTY\_KILL: character to erase the entire line
* UARTTY\_ICRNL: convert carriage return to linefeed on input
* UARTTY\_INLCR: convert linefeed to carriage return on input
* UARTTY\_IMAXBEL: ring the bell when the input buffer is full
* UARTTY\_IGNCR: ignore carriage return on input
* UARTTY\_ISTRIP: strip the eighth bit on input
* UARTTY\_IXON: enable XON/XOFF flow control
* UARTTY\_OPOST: enable output post-processing
* UARTTY\_ONLCR: convert linefeed to carriage return linefeed on output
* UARTTY\_OCRNL: convert carriage return to linefeed on output

Enabling/disabling these options affects the generated code size:
* 268 with "raw" preset
* 1142 with "sane" preset
(These are only the common configurations. A complete list of sizes for
each combination of options would be prohibitively large.)

UARTTY also supports both blocking and non-blocking getc and putc
functions (one or both of these options must be enabled):
* UARTTY\_NONBLOCK: build non-blocking getc and putc
* UARTTY\_BLOCK: build blocking getc and putc

The code sizes listed above are based on blocking mode only.
Non-blocking mode adds 94 bytes in raw mode and 74 bytes in sane mode.

Buffer size options:
* UARTTY\_TX\_BUF\_SIZE: size of output buffer (default 16)
* UARTTY\_RX\_BUF\_SIZE: size of input buffer (default 256)

Note: POSIX specifies that the input buffer must be at least 255 bytes.
An AVR application may not need an input buffer that large, especially
if it uses raw mode. UARTTY will function properly with a smaller input
buffer but will display a warning at compile time. (A smaller input
buffer also adds a few bytes to the code size because UARTTY must mask
the buffer indices.)

## Contributors

TODO

## License

TODO

## How to pronounce UARTTY

I say it like "you-arty", but you can say it like "you-are-titty" if you
prefer.

