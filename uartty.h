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

/* begin configuration options */

#define UARTTY_TX_BUF_SIZE 16
#define UARTTY_RX_BUF_SIZE 256

//#define UARTTY_PRESET UARTTY_PRESET_RAW
#define UARTTY_PRESET UARTTY_PRESET_SANE

// define individual options if no preset is selected
#ifndef UARTTY_PRESET_UARTTY

// Local flags (c_lflag)
// ICANON: enable canonical mode
#define UARTTY_ICANON 1
// IEXTEN: enable extended input processing
#define UARTTY_IEXTEN 1
// ECHO: echo input characters
#define UARTTY_ECHO 1
// ECHOE: erase character erases the preceding input character, and werase
// character erases the preceding word
#define UARTTY_ECHOE 1
// ECHOK: kill character erases the current line
#define UARTTY_ECHOK 1
// ECHONL: echo NL even if ECHO is not set
//#define UARTTY_ECHONL 1
// ECHOCTL: echo ctrl characters as ^X
#define UARTTY_ECHOCTL 1

// Control characters (c_cc)
#define UARTTY_ERASE  0x7f
#define UARTTY_WERASE CTRL('W')
#define UARTTY_KILL   CTRL('U')
//#define UARTTY_REPRINT 1 // not implemented

// Input modes (c_iflag)
// ICRNL: convert carriage return to linefeed on input
#define UARTTY_ICRNL 1
// INLCR: convert linefeed to carriage return on input
//#define UARTTY_INLCR 1
// IMAXBEL: ring bell when input queue is full
#define UARTTY_IMAXBEL 1
// IGNCR: ignore carriage return on input
//#define UARTTY_IGNCR 1
// ISTRIP: strip off eighth bit
//#define UARTTY_ISTRIP 1
// IXON: enable XON/XOFF flow control
#define UARTTY_IXON 1

// Output modes (c_oflag)
// OPOST: perform post-processing
#define UARTTY_OPOST 1
// ONLCR: convert linefeed to carriage return linefeed on output
#define UARTTY_ONLCR 1
// OCRNL: convert carriage return to linefeed on output
//#define UARTTY_OCRNL 1

#endif


// NONBLOCK: enable non-blocking getc and putc
//#define UARTTY_NONBLOCK 1
// BLOCK: enable blocking getc and putc
#define UARTTY_BLOCK 1

/* end configuration options */

void uartty_init(unsigned int baudrate);
int uartty_getc(FILE *unused);
int uartty_getc_nb(FILE *unused);
int uartty_putc(char data, FILE *unused);
int uartty_putc_nb(char data, FILE *unused);
//void uartty_puts(const char *s);
//void uartty_puts_p(const char *s);

#endif
