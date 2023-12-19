/*
  logging.c - Serial logging for the Arduino Uno in C
  Copyright (c) 2006 Rasmus Källqvist.  All right reserved.

  logging.c is heavily based on HardwareSerial.cpp and
  HardwareSerial0.cpp by Nicholas Zambetti.

  ------------------------------------------------------------

  HardwareSerial.cpp - Hardware serial library for Wiring
  Copyright (c) 2006 Nicholas Zambetti.  All right reserved.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

  Modified 23 November 2006 by David A. Mellis
  Modified 28 September 2010 by Mark Sproul
  Modified 14 August 2012 by Alarus
  Modified 3 December 2013 by Matthijs Kooijman
  Modified 18 December 2023 by Rasmus Källqvist
*/

#include <rkcerial/logging.h>

#include <avr/io.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util/atomic.h>

#define clear_bit(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define set_bit(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))

/* ----------------------------- String utility ----------------------------- */
static bool string_starts_with(const char* str, const char* prefix) {
	return strncmp(prefix, str, strlen(prefix)) == 0;
}

static const char* file_name_from_path(const char* path) {
	const char* file_name = path;
	while (*(path++)) {
		if (*path == '/' || *path == '\\') {
			file_name = path + 1;
		}
	}
	return file_name;
}

/* ------------------------------- Serial IO -------------------------------- */
#define SERIAL_RING_BUFFER_SIZE 64

typedef struct {
	volatile uint8_t head;
	volatile uint8_t tail;
	uint8_t buffer[SERIAL_RING_BUFFER_SIZE];
} ringbuffer_t;

typedef struct {
	ringbuffer_t rx;
	ringbuffer_t tx;
} serial_t;

static serial_t g_serial = { 0 };
static unsigned long (*g_time_now_ms)(void);

static unsigned long time_now_ms_noop(void) {
	return 0;
}

static void rx_complete_irq(void) {
	const uint8_t byte = UDR0;
	const bool parity_error = bit_is_set(UCSR0A, UPE0);
	if (parity_error) {
		return; // Parity error, discard read byte
	}

	uint8_t next_index = (g_serial.rx.head + 1) % SERIAL_RING_BUFFER_SIZE;
	if (next_index == g_serial.rx.tail) {
		return; // About to overflow, discard read byte
	}

	// Write read byte to buffer
	g_serial.rx.buffer[g_serial.rx.head] = byte;
	g_serial.rx.head = next_index;
}

ISR(USART_RX_vect) {
	rx_complete_irq();
}

static void tx_udr_empty_irq(void) {
	// If interrupts are enabled, there must be more data in the output
	// buffer. Send the next byte
	unsigned char byte = g_serial.tx.buffer[g_serial.tx.tail];
	g_serial.tx.tail = (g_serial.tx.tail + 1) % SERIAL_RING_BUFFER_SIZE;

	UDR0 = byte;

	if (g_serial.tx.head == g_serial.tx.tail) {
		// Buffer empty, so disable interrupts
		clear_bit(UCSR0B, UDRIE0);
	}
}

ISR(USART_UDRE_vect) {
	tx_udr_empty_irq();
}

static void serial_initialize(int baud) {
	// enable "double the USART transmission speed"
	UCSR0A = 1 << U2X0;

	// set the baud rate
	const uint16_t baud_setting = (F_CPU / 4 / baud - 1) / 2;
	UBRR0L = baud_setting;
	UBRR0H = baud_setting >> 8;

	set_bit(UCSR0B, RXEN0); // enable UART Rx
	set_bit(UCSR0B, TXEN0); // enable UART Tx
	set_bit(UCSR0B, RXCIE0); // enable receive interrupts
	clear_bit(UCSR0B, UDRIE0); // disable data register empty interrupts
}

static int serial_read_byte(serial_t* serial) {
	// if the head isn't ahead of the tail, we don't have any characters
	if (serial->rx.head == serial->rx.tail) {
		return -1;
	}

	uint8_t byte = serial->rx.buffer[serial->rx.tail];
	serial->rx.tail = (serial->rx.tail + 1) % SERIAL_RING_BUFFER_SIZE;
	return byte;
}

static int serial_read_byte_with_timeout(serial_t* serial) {
	const unsigned long timeout_ms = 1000;
	const unsigned long start_ms = g_time_now_ms();
	int byte;
	do {
		byte = serial_read_byte(serial);
		if (byte >= 0) {
			return byte;
		}
	} while (g_time_now_ms() - start_ms < timeout_ms);
	return -1; // timed out
}

static void serial_read_string(serial_t* serial, char* str_buf, size_t str_buf_len) {
	int index = 0;
	int byte = serial_read_byte_with_timeout(serial);
	while (byte >= 0 && index < str_buf_len) {
		str_buf[index++] = (char)byte;
		byte = serial_read_byte_with_timeout(serial);
	}
}

static void serial_write(serial_t* serial, uint8_t byte) {
	uint8_t next_index = (serial->tx.head + 1) % SERIAL_RING_BUFFER_SIZE;
	serial->tx.buffer[serial->tx.head] = byte;

	// If the output buffer is full, there's nothing for it other than to
	// wait for the interrupt handler to empty it a bit
	while (next_index == serial->tx.tail) {
		if (bit_is_clear(SREG, SREG_I)) {
			// Interrupts are disabled, so we'll have to poll the data
			// register empty flag ourselves. If it is set, pretend an
			// interrupt has happened and call the handler to free up
			// space for us.
			if (bit_is_set(UCSR0A, UDRE0)) {
				tx_udr_empty_irq();
			}
		} else {
			// nop, the interrupt handler will free up space for us
		}
	}

	// make atomic to prevent execution of ISR between setting the
	// head pointer and setting the interrupt flag resulting in buffer
	// retransmission
	ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
		serial->tx.head = next_index;
		set_bit(UCSR0B, UDRIE0);
	}
}

static void serial_print(serial_t* serial, const char* str) {
	while (*str) {
		serial_write(serial, *str);
		str++;
	}
}

static uint8_t serial_num_available_bytes(serial_t* serial) {
	return (SERIAL_RING_BUFFER_SIZE + serial->rx.head - serial->rx.tail) % SERIAL_RING_BUFFER_SIZE;
}

/* ------------------------------- Public API ------------------------------- */
#define COLOR_GREEN "\033[32m"
#define COLOR_RED "\033[31m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_RESET "\033[0m"

static unsigned long g_ms_since_midnight = 0;

static const char* log_level_str[] = {
	"INFO",
	"WARNING",
	"ERROR",
};

static const char* log_level_color[] = {
	COLOR_GREEN,
	COLOR_YELLOW,
	COLOR_RED,
};

static int snprintf_time(char* str_buf, size_t str_buf_len) {
	unsigned long now_ms = g_ms_since_midnight + g_time_now_ms();
	unsigned long hour = (now_ms / (1000L * 60L * 60L)) % 24L;
	unsigned long minutes = (now_ms / (1000L * 60L)) % 60L;
	unsigned long seconds = (now_ms / 1000L) % 60L;
	unsigned long milliseconds = now_ms % 1000L;

	return snprintf(str_buf, str_buf_len, "%02lu:%02lu:%02lu:%03lu", hour, minutes, seconds, milliseconds);
}

static void rk_printf(const char* fmt, ...) {
	char str[128];
	va_list args;
	va_start(args, fmt);
	vsnprintf(str, 128, fmt, args);
	va_end(args);

	serial_print(&g_serial, str);
}

void rk_init_logging(time_now_ms_t callback) {
	g_time_now_ms = callback ? callback : time_now_ms_noop;
	serial_initialize(9600);

	if (!callback) {
		rk_printf("[ Logging ] Logging initialized. (Timestamps disabled).\n");
		return;
	}

	rk_printf("[ Logging ] Logging initialized, waiting for wall clock time.\n");

	char input_buf[64] = { 0 };
	const unsigned long timeout_ms = 100;
	const unsigned long start_ms = g_time_now_ms();
	while (true) {
		/* Read input, look for clock time */
		serial_read_string(&g_serial, input_buf, 64);
		if (string_starts_with(input_buf, "TIMENOW")) {
			/* Received clock time */
			int offset = strlen("TIMENOW ");
			g_ms_since_midnight = strtol(input_buf + offset, NULL, 10);
			rk_printf("[ Logging ] Received wall clock time.\n", g_ms_since_midnight);
			break;
		}

		/* Timed out, abort */
		const unsigned long now_ms = g_time_now_ms();
		if (now_ms - start_ms > timeout_ms) {
			rk_printf("[ Logging ] Timed out on getting wall clock time (waited %lu milliseconds).\n", timeout_ms);
			break;
		}
	}
}

void rk_log(rk_log_level_t level, const char* file, int line, const char* fmt, ...) {
	char str[128];
	int offset = 0;

	/* Print prefix */
	offset += snprintf(str + offset, 128 - offset, "[");
	if (g_time_now_ms != time_now_ms_noop) {
		offset += snprintf_time(str + offset, 128 - offset);
		offset += snprintf(str + offset, 128 - offset, " ");
	}
	offset += snprintf(str + offset, 128 - offset, "%s%s%s %s:%d] ", log_level_color[level], log_level_str[level], COLOR_RESET, file_name_from_path(file), line);

	/* Print user string */
	va_list args;
	va_start(args, fmt);
	vsnprintf(str + offset, 128 - offset, fmt, args);
	va_end(args);

	serial_print(&g_serial, str);
}
