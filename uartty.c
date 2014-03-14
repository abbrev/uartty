#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <avr/interrupt.h>
#include <avr/io.h>

#include "uartty.h"

#if !UARTTY_BLOCK && !UARTTY_NONBLOCK
#error UARTTY_BLOCK or UARTTY_NONBLOCK (or both) must be enabled
#endif

// preset modes
#define UARTTY_PRESET_RAW  1 // like cfmakeraw()
#define UARTTY_PRESET_COOKED 2 // like stty cooked
#define UARTTY_PRESET_SANE 3 // like stty sane

#ifndef UARTTY_PRESET
// do nothing
#elif UARTTY_PRESET == UARTTY_PRESET_RAW

# undef UARTTY_ISTRIP
# undef UARTTY_INLCR
# undef UARTTY_IGNCR
# undef UARTTY_ICRNL
# undef UARTTY_IXON
# undef UARTTY_OPOST
# undef UARTTY_ECHO
# undef UARTTY_ECHONL
# undef UARTTY_ICANON
# undef UARTTY_IMAXBEL

#elif UARTTY_PRESET == UARTTY_PRESET_COOKED

//       cooked same as brkint ignpar istrip icrnl ixon opost isig  icanon,  eof
//              and eol characters to their default values

# define UARTTY_ICRNL 1
# define UARTTY_IXON 1
# define UARTTY_OPOST 1
# define UARTTY_ICANON 1

#elif UARTTY_PRESET == UARTTY_PRESET_SANE

//       sane   same  as  cread -ignbrk brkint -inlcr -igncr icrnl -iutf8 -ixoff
//              -iuclc -ixany imaxbel opost -olcuc -ocrnl onlcr  -onocr  -onlret
//              -ofill  -ofdel  nl0 cr0 tab0 bs0 vt0 ff0 isig icanon iexten echo
//              echoe echok -echonl  -noflsh  -xcase  -tostop  -echoprt  echoctl
//              echoke, all special characters to their default values

# undef UARTTY_INLCR
# undef UARTTY_IGNCR
# define UARTTY_ICRNL 1
# define UARTTY_IMAXBEL 1
# define UARTTY_OPOST 1
# undef UARTTY_OCRNL
# define UARTTY_ONLCR 1
# define UARTTY_ICANON 1
# define UARTTY_IEXTEN 1
# define UARTTY_ECHO 1
# define UARTTY_ECHOE 1
# define UARTTY_ECHOK 1
# undef UARTTY_ECHONL
# define UARTTY_ECHOCTL 1
# define UARTTY_ERASE  0x7f
# define UARTTY_WERASE CTRL('W')
# define UARTTY_KILL   CTRL('U')

#else
# error unknown value for UARTTY_PRESET
#endif




// control characters are disabled in non-canonical mode
#if !UARTTY_ICANON
# undef UARTTY_ERASE
# undef UARTTY_WERASE
# undef UARTTY_KILL
# undef UARTTY_REPRINT
#endif

// all output processing is disabled if OPOST is not set
#if !UARTTY_OPOST
# undef UARTTY_ONLCR
# undef UARTTY_OCRNL
#endif

// WERASE is disabled if IEXTEN is not set
#if !UARTTY_IEXTEN
# undef UARTTY_WERASE
#endif

// disable ECHO* if ECHO is not set
#if !UARTTY_ECHO
# undef UARTTY_ECHOE
# undef UARTTY_ECHOK
# undef UARTTY_ECHOCTL
#endif

// atmega328p
#define ATMEGA_USART0
#define UART0_RECEIVE_INTERRUPT   USART_RX_vect
#define UART0_TRANSMIT_INTERRUPT  USART_UDRE_vect
#define UART0_STATUS   UCSR0A
#define UART0_CONTROL  UCSR0B
#define UART0_DATA     UDR0
#define UART0_UDRIE    UDRIE0


#ifndef UARTTY_TX_BUF_SIZE
# warning Using default value for UARTTY_TX_BUF_SIZE
# define UARTTY_TX_BUF_SIZE 16
#endif

#ifndef UARTTY_RX_BUF_SIZE
# warning Using default value for UARTTY_RX_BUF_SIZE
# define UARTTY_RX_BUF_SIZE 256
#endif

#define UARTTY_TX_BUF_MASK (UARTTY_TX_BUF_SIZE - 1)
#define UARTTY_RX_BUF_MASK (UARTTY_RX_BUF_SIZE - 1)

#if UARTTY_TX_BUF_SIZE & UARTTY_TX_BUF_MASK
# error UARTTY_TX_BUF_SIZE must be a power of two
#endif

#if UARTTY_RX_BUF_SIZE & UARTTY_RX_BUF_MASK
# error UARTTY_RX_BUF_SIZE must be a power of two
#endif

// we sacrifice one byte in our queues so the queues must be at least 2 bytes
#if UARTTY_TX_BUF_SIZE < 2 || UARTTY_TX_BUF_SIZE > 256
# error UARTTY_TX_BUF_SIZE must be between 2 and 256
#endif

#if UARTTY_RX_BUF_SIZE < 2 || UARTTY_RX_BUF_SIZE > 256
# error UARTTY_RX_BUF_SIZE must be between 2 and 256
#endif

// _POSIX_MAX_CANON is 255
#if UARTTY_RX_BUF_SIZE < 256
# warning Receive buffer is smaller than
# warning _POSIX_MAX_CANON (this matters
# warning only for applications that wish
# warning to have a POSIX-compliant TTY).
#endif





// I call the indices "get" and "put" so they're unambiguous as "head" and
// "tail" might be.
#define QUEUE(s) struct { unsigned char get, put; char buf[s]; }

static volatile QUEUE(UARTTY_TX_BUF_SIZE) txq;
static volatile QUEUE(UARTTY_RX_BUF_SIZE) rxq;
#if UARTTY_ICANON
static volatile unsigned char inlcount; // XXX this may need to be protected
#endif

#define RXQEMPTY() (rxq.get == rxq.put)
#define TXQEMPTY() (txq.get == txq.put)

// send a character and return the character
// return -1 if queue is full
static int tx_put(char data)
{
	unsigned char put = (txq.put + 1) & UARTTY_TX_BUF_MASK;
	if (put == txq.get) return -1;

	txq.buf[put] = data;
	return (unsigned char)data;
}

// unreceive a character and return the character unreceived
// return -1 if rx queue is empty
static int rx_unput(void)
{
	if (RXQEMPTY()) return -1;
	unsigned char put = (rxq.put - 1) & UARTTY_RX_BUF_MASK;
	char data = rxq.buf[put];
	rxq.put = put;
	return (unsigned char)data;
}

// receive a character and return the character received
// return -1 if rx queue is full
static int rx_put(char data)
{
	unsigned char put = (rxq.put + 1) & UARTTY_RX_BUF_MASK;
	//unsigned char put2 = (rxq.put + 2) & UARTTY_RX_BUF_MASK;
	if (rxq.get == put) return -1; // full
	//if (data != '\n' && rxq.get == put2) return -1;
	rxq.buf[rxq.put] = data;
	rxq.put = put;
	return (unsigned char)data;
}

/*
 * For removing characters from the receive queue and sending characters
 * to erase those characters on the remote side, the transmit interrupt
 * participates in the sending of erase characters.
 *
 * In the receive interrupt, when an erase character is received, erase
 * n characters from the receive queue (where 0 <= n <= 1 for backspace
 * and 0 <= n for ^U) and then add n to the erase counter.
 *
 * In the transmit interrupt, if the erase counter is non-zero, send a
 * backspace-space-backspace combination and decrement the erase
 * counter. Otherwise send a character from the transmit queue. Since
 * the transmit interrupt must send three characters to erase a
 * character, the transmit interrupt can maintain an erase state, which
 * is one of three states (backspace 1, space, backspace 2).
 */

static volatile unsigned char erase_count;

#define CTRL(x) ((x) - 0x40)

#if UARTTY_ICANON
// return true if character is erased
static bool erase_if_not(bool (*cond)(int c), bool kill)
{
	int u = rx_unput();
	if (u == '\n' || cond(u)) {
		rx_put(u);
	} else if (u != -1) {
#if UARTTY_ECHOE || UARTTY_ECHOK
#if UARTTY_ECHOK && !UARTTY_ECHOE
		if (kill) {
#elif !UARTTY_ECHOK && UARTTY_ECHOE
		if (!kill) {
#else
		{
#endif
		// TODO protect erase_count
			unsigned char e = erase_count;
			++e;
#if UARTTY_ECHOCTL
			if (!isprint(u))
				++e;
#endif
			erase_count = e;
		}
#endif
		return true;
	}
	return false;
}

static void erase_line_until(bool (*cond)(int c), bool kill)
{
	while (erase_if_not(cond, kill))
		;
}

static bool is_space(int c) { return c == ' '; }
static bool is_not_space(int c) { return c != ' '; }
static bool never(int c) { return false; }
#endif

static void echo(char c)
{
#if UARTTY_ECHO || UARTTY_ECHONL
	if (c == '\n') {
		tx_put('\n');
	}
#endif

#if UARTTY_ECHO
#  if UARTTY_ECHOCTL
	else if (!isprint(c)) {
		// echoctl
		// print non-printable characters as ^X where X
		// is c xor 0x40
		tx_put('^');
		tx_put(c^0x40);
	}
#  endif
	else {
		tx_put(c);
	}
#endif
}

static volatile bool halt_output = false;

#define DISABLE_TX_INTERRUPT() (UART0_CONTROL &= ~(1<<UART0_UDRIE))
#define  ENABLE_TX_INTERRUPT() (UART0_CONTROL |= (1<<UART0_UDRIE))

ISR(UART0_RECEIVE_INTERRUPT)
{
	char data = UART0_DATA;

#if UARTTY_IXON
	if (data == CTRL('S')) {
		halt_output = true;
		return;
	} else if (data == CTRL('Q')) {
		halt_output = false;
		ENABLE_TX_INTERRUPT();
		return;
	}
#endif

#if UARTTY_ISTRIP
	data &= 0x7f;
#endif

#if UARTTY_IGNCR
	if (data == '\r') return;
#endif

#if UARTTY_ICRNL
	if (data == '\r') data = '\n'; else
#endif
#if UARTTY_INLCR
	if (data == '\n') data = '\r';
#else
	;
#endif

	unsigned char put = (rxq.put + 1) & UARTTY_RX_BUF_MASK;
	//unsigned char put2 = (rxq.put + 2) & UARTTY_RX_BUF_MASK;

#if UARTTY_ERASE
	if (data == '\b' || data == UARTTY_ERASE) {
		// erase = ^?
		// (BS is also supported)
		erase_if_not(never, false);
#if !UARTY_ECHOE
		echo(data);
#endif
	} else
#endif
#if UARTTY_KILL
	if (data == CTRL('U')) {
		// kill = ^U
		erase_line_until(never, true);
#if !UARTY_ECHOK
		echo(data);
#endif
	} else
#endif
#if UARTTY_WERASE
	if (data == CTRL('W')) {
		// werase = ^W
		// gobble spaces
		erase_line_until(is_not_space, false);
		// gobble non-spaces
		erase_line_until(is_space, false);
#if !UARTY_ECHOE
		echo(data);
#endif
	} else
#endif
#if 0 // not implemented
#if UARTTY_REPRINT
	if (data == CTRL('R')) {
		// rprnt = ^R
		/*
		 * we could have the transmit interrupt participate in
		 * reprinting the receive buffer somehow
		 */
	} else
#endif
#endif
#if UARTTY_ICANON
	if (put != rxq.get &&
		 (data == '\n' || ((put+1)&UARTTY_RX_BUF_MASK) != rxq.get)) {
	//negated: if (put == rxq.get || (data != '\n' && put2 == rxq.get)) {
		// limit line to 2 less than queue size if character is not a
		// newline character or to 1 less than queue size if it is
#if UARTTY_ICANON
		if (data == '\n') ++inlcount;
#endif
#else
	if (put != rxq.get) {
#endif
		rxq.buf[put] = data;
		rxq.put = put;

		echo(data);
	} else {
#if UARTTY_IMAXBEL
		// imaxbel
		// send a BEL to indicate full buffer
		tx_put('\a');
#endif
	}
}

ISR(UART0_TRANSMIT_INTERRUPT)
{
#if UARTTY_IXON
	if (halt_output) {
		DISABLE_TX_INTERRUPT();
	}
#endif
#if UARTTY_ONLCR
	static bool sendlf = false;

	if (sendlf) {
		UART0_DATA = '\n';
		sendlf = false;
		return;
	}
#endif

#if UARTTY_ECHOE || UARTTY_ECHOK
	static char erase_state;

	// TODO protect erase_count
	if (erase_state == 0 && erase_count != 0) {
		--erase_count;
		erase_state = 1;
	}

	// echoe
	// erase characters with backspace-space-backspace
	if (erase_state == 1) {
		UART0_DATA = '\b';
		++erase_state;
		return;
	} else if (erase_state == 2) {
		UART0_DATA = ' ';
		++erase_state;
		return;
	} else if (erase_state == 3) {
		UART0_DATA = '\b';
		erase_state = 0;
		return;
	}
#endif

	if (txq.put != txq.get) {
		unsigned char get = (txq.get + 1) & UARTTY_TX_BUF_MASK;
		char data = txq.buf[txq.get];
		txq.get = get;
#if UARTTY_OCRNL
		if (data == '\r') data = '\n';
#endif
#if UARTTY_ONLCR
		if (data == '\n') {
			data = '\r';
			sendlf = true;
		}
#endif
		UART0_DATA = data;
	} else {
		DISABLE_TX_INTERRUPT();
	}
}

void uartty_init(unsigned int ubrr)
{
	// TODO
}

static int rxget(void)
{
#if UARTTY_ICANON
	if (!inlcount) return -1;
#endif

	unsigned char get = (txq.get + 1) & UARTTY_RX_BUF_MASK;
	char data = rxq.buf[rxq.get];
	rxq.get = get;
#if UARTTY_ICANON
	if (data == '\n') --inlcount;
#endif
	return (unsigned char)data;
}

#if UARTTY_NONBLOCK
int uartty_getc_nb(FILE *unused)
{
	return rxget();
}
#endif

// TODO
#define SLEEP() do ; while (0)

#if UARTTY_BLOCK
int uartty_getc(FILE *unused)
{
	(void)unused;
	int c;

	while ((c = rxget()) < 0) {
		SLEEP();
	}
	return c;
}
#endif

#if UARTTY_NONBLOCK
// non-blocking version
int uartty_putc_nb(char data, FILE *unused)
{
	return tx_put(data);
}
#endif

#if UARTTY_BLOCK
int uartty_putc(char data, FILE *unused)
{
	while (tx_put(data) < 0) {
		SLEEP();
	}
	return (unsigned char)data;
}
#endif

