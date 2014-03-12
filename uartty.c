#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <avr/interrupt.h>
#include <avr/io.h>

#include "uartty.h"

/*
 * uartty configuration options
 * XXX ICANON is always assumed to be enabled because that is the main point of
 * this library!
 */

// Control characters (c_cc)
#define UARTTY_ERASE  0x7f
#define UARTTY_WERASE CTRL('W')
#define UARTTY_KILL   CTRL('U')
//#define UARTTY_RPRNT 1 // not implemented

// Input modes (c_iflag)
#define UARTTY_ICRNL 1
#define UARTTY_IMAXBEL 1 // ring bell when input queue is full

// Output modes (c_oflag)
#define UARTTY_ONLCR 1

// Local flags (c_lflag)
// ECHO: echo input characters
#define UARTTY_ECHO 1
// ECHOE: erase character erases the preceding input character, and werase
// character erases the preceding word
#define UARTTY_ECHOE 1
// ECHOK: kill character erases the current line
#define UARTTY_ECHOK 1
// ECHONL: echo NL even if !ECHO
#define UARTTY_ECHONL 0
// ECHOCTL: echo ctrl characters as ^X
#define UARTTY_ECHOCTL 1
// ECHOKE: kill erases each character on the line
#define UARTTY_ECHOKE 1 // XXX not used


// atmega328p
#define ATMEGA_USART0
#define UART0_RECEIVE_INTERRUPT   USART_RX_vect
#define UART0_TRANSMIT_INTERRUPT  USART_UDRE_vect
#define UART0_STATUS   UCSR0A
#define UART0_CONTROL  UCSR0B
#define UART0_DATA     UDR0
#define UART0_UDRIE    UDRIE0


#define UARTTY_TX_BUF_SIZE (1<<UARTTY_LOG2_TX_BUF_SIZE)
#define UARTTY_TX_BUF_MASK (UARTTY_TX_BUF_SIZE - 1)
#define UARTTY_RX_BUF_SIZE (1<<UARTTY_LOG2_RX_BUF_SIZE)
#define UARTTY_RX_BUF_MASK (UARTTY_RX_BUF_SIZE - 1)

// I call the indices "get" and "put" so they're unambiguous as "head" and
// "tail" might be.
#define QUEUE(s) struct { unsigned char get, put; char buf[s]; }

volatile QUEUE(UARTTY_TX_BUF_SIZE) txq;
volatile QUEUE(UARTTY_RX_BUF_SIZE) rxq;
volatile unsigned char inlcount; // XXX this may need to be protected

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
	unsigned char put2 = (rxq.put + 2) & UARTTY_RX_BUF_MASK;
	if (rxq.get == put) return -1; // full
	if (data != '\n' && rxq.get == put2) return -1;
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

static volatile unsigned char erase_count = 0;

#define CTRL(x) ((x) - 0x40)

// return true if character is erased
static bool erase_if_not(bool (*cond)(int c))
{
	int u = rx_unput();
	if (u == '\n' || cond(u)) {
		rx_put(u);
	} else if (u != -1) {
#if UARTTY_ECHO
		// TODO protect erase_count
		{
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

static void erase_line_until(bool (*cond)(int c))
{
	while (erase_if_not(cond))
		;
}

static bool is_space(int c) { return c == ' '; }
static bool is_not_space(int c) { return c != ' '; }
static bool never(int c) { return false; }

ISR(UART0_RECEIVE_INTERRUPT)
{
	char data = UART0_DATA;

#if UARTTY_ICRNL
	// icrnl
	if (data == '\r') data = '\n';
#endif

	unsigned char put = (rxq.put + 1) & UARTTY_RX_BUF_MASK;
	unsigned char put2 = (rxq.put + 2) & UARTTY_RX_BUF_MASK;

#if UARTTY_ERASE
	if (data == '\b' || data == UARTTY_ERASE) {
		// erase = ^?
		// (BS is also supported)
		erase_if_not(never);
	} else
#endif
#if UARTTY_KILL
	if (data == CTRL('U')) {
		// kill = ^U
		// TODO ECHOK/ECHOKE
		erase_line_until(never);
	} else
#endif
#if UARTTY_WERASE
	if (data == CTRL('W')) {
		// werase = ^W
		// gobble spaces
		erase_line_until(is_not_space);
		// gobble non-spaces
		erase_line_until(is_space);
	} else
#endif
#if 0 // not implemented
#if UARTTY_RPRNT
	if (data == CTRL('R')) {
		// rprnt = ^R
		/*
		 * we could have the transmit interrupt participate in
		 * reprinting the receive buffer somehow
		 */
	} else
#endif
#endif
	if (put != rxq.get && (data == '\n' || put2 != rxq.get)) {
	//negated: if (put == rxq.get || (data != '\n' && put2 == rxq.get)) {
		// limit line to 2 less than queue size if character is not a
		// newline character or to 1 less than queue size if it is
		rxq.buf[put] = data;
		rxq.put = put;
		if (data == '\n') ++inlcount;
#if UARTTY_ECHO
#if UARTTY_ECHOCTL
		if (isprint(data)) {
			tx_put(data);
		} else {
			// echoctl
			// print non-printable characters as ^x where x
			// is the character + 0x40
			tx_put('^');
			tx_put(data+0x40);
		}
#else
		tx_put(data);
#endif
#elif UARTTY_ECHONL
		if (data == '\n') {
			tx_put(data);
		}
#endif
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
#if UARTTY_ECHOE || UARTTY_ECHOK
	static char erase_state = 0;

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
		UART0_DATA = txq.buf[txq.get];
		txq.get = get;
	} else {
		UART0_CONTROL &= ~(1<<UART0_UDRIE);
	}
}

void uartty_init(unsigned int baudrate)
{
	// TODO
}

static int rxget(void)
{
	if (!inlcount) return -1;

	unsigned char get = (txq.get + 1) & UARTTY_RX_BUF_MASK;
	char data = rxq.buf[rxq.get];
	rxq.get = get;
	if (data == '\n') --inlcount;
	return (unsigned char)data;
}

int uartty_getc_nb(FILE *unused)
{
	return rxget();
}

int uartty_getc(FILE *unused)
{
	(void)unused;
	int c;

	while ((c = rxget()) < 0) {
		// TODO sleep
	}
	return c;
}

// non-blocking version
int uartty_putc_nb(char data, FILE *unused)
{
#if UARTTY_ONLCR
	// onlcr
	if (data == '\n') uartty_putc_nb('\r', unused);
#endif
	return tx_put(data);
}

int uartty_putc(char data, FILE *unused)
{
#if UARTTY_ONLCR
	// onlcr
	if (data == '\n') uartty_putc('\r', unused);
#endif

	while (tx_put(data) < 0) {
		// TODO sleep
	}
	return (unsigned char)data;
}

