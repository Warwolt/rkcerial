#include <rkcerial/logging.h>

#include <arduino/USBAPI.h>
#include <avr/io.h>
#include <stdio.h>

#include <HardwareSerial_private.h>

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

/* --------------------------------- Timing --------------------------------- */
// the prescaler is set so that timer0 ticks every 64 clock cycles, and the
// the overflow handler is called every 256 ticks.
#define MICROSECONDS_PER_TIMER0_OVERFLOW (clockCyclesToMicroseconds(64 * 256))

// the whole number of milliseconds per timer0 overflow
#define MILLIS_INC (MICROSECONDS_PER_TIMER0_OVERFLOW / 1000)

// the fractional number of milliseconds per timer0 overflow. we shift right
// by three to fit these numbers into a byte. (for the clock speeds we care
// about - 8 and 16 MHz - this doesn't lose precision.)
#define FRACT_INC ((MICROSECONDS_PER_TIMER0_OVERFLOW % 1000) >> 3)
#define FRACT_MAX (1000 >> 3)

static volatile unsigned long g_timer0_overflow_count = 0;
static volatile unsigned long g_timer0_millis = 0;
static unsigned char g_timer0_fract = 0;

ISR(TIMER0_OVF_vect) {
	// copy these to local variables so they can be stored in registers
	// (volatile variables must be read from memory on every access)
	unsigned long m = g_timer0_millis;
	unsigned char f = g_timer0_fract;

	m += MILLIS_INC;
	f += FRACT_INC;
	if (f >= FRACT_MAX) {
		f -= FRACT_MAX;
		m += 1;
	}

	g_timer0_fract = f;
	g_timer0_millis = m;
	g_timer0_overflow_count++;
}

// Configures Timer 0 to be used for counting elapsed milliseconds
static void timer_initialize() {
	// Set prescale factor to be 64
	set_bit(TCCR0B, CS01);
	set_bit(TCCR0B, CS00);

	// Enable timer 0 overflow interrupt
	set_bit(TIMSK0, TOIE0);
}

// returns num elapsed milliseconds since program start
static unsigned long time_now_ms() {
	unsigned long now_ms;
	uint8_t old_SREG = SREG;

	// disable interrupts while we read g_timer0_millis or we might get an
	// inconsistent value (e.g. in the middle of a write to g_timer0_millis)
	cli();
	now_ms = g_timer0_millis;
	SREG = old_SREG;

	return now_ms;
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

ISR(USART_RX_vect) {
	const uint8_t byte = UDR0;
	const bool parity_error = bit_is_set(UCSR0A, UPE0);
	if (parity_error) {
		return; // Parity error, discard read byte
	}

	uint8_t next_index = (g_serial.rx.head + 1) % SERIAL_RX_BUFFER_SIZE;
	if (next_index == g_serial.rx.tail) {
		return; // About to overflow, discard read byte
	}

	// Write read byte to buffer
	g_serial.rx.buffer[g_serial.rx.head] = byte;
	g_serial.rx.head = next_index;
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
	serial->rx.tail = (serial->rx.tail + 1) % SERIAL_RX_BUFFER_SIZE;
	return byte;
}

static int serial_read_byte_with_timeout(serial_t* serial) {
	const unsigned long timeout_ms = 1000;
	const unsigned long start_ms = time_now_ms();
	int byte;
	do {
		byte = serial_read_byte(serial);
		if (byte >= 0) {
			return byte;
		}
	} while (time_now_ms() - start_ms < timeout_ms);
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

static void serial_write(uint8_t byte) {
	Serial.write(byte);
}

static void serial_print(const char* str) {
	while (*str) {
		serial_write(*str);
		str++;
	}
}

static int serial_num_available_bytes(void) {
	return Serial.available();
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
	unsigned long now_ms = g_ms_since_midnight + time_now_ms();
	unsigned long hour = (now_ms / (1000L * 60L * 60L)) % 24L;
	unsigned long minutes = (now_ms / (1000L * 60L)) % 60L;
	unsigned long seconds = (now_ms / 1000L) % 60L;
	unsigned long milliseconds = now_ms % 1000L;

	return snprintf(str_buf, str_buf_len, "%02lu:%02lu:%02lu:%03lu", hour, minutes, seconds, milliseconds);
}

void rk_init_logging(void) {
	sei(); // globally enable interrupts
	timer_initialize();
	serial_initialize(9600);

	rk_printf("[ Logging ] Logging initialized, waiting for wall clock time.\n");

	char input_buf[64] = { 0 };
	const unsigned long timeout_ms = 2000;
	const unsigned long start_ms = time_now_ms();
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
		const unsigned long now_ms = time_now_ms();
		if (now_ms - start_ms > timeout_ms) {
			rk_printf("[ Logging ] Timed out on getting wall clock time (waited %lu milliseconds).\n", timeout_ms);
			break;
		}
	}
}

void rk_printf(const char* fmt, ...) {
	char str[128];
	va_list args;
	va_start(args, fmt);
	vsnprintf(str, 128, fmt, args);
	va_end(args);

	serial_print(str);
}

void rk_log(rk_log_level_t level, const char* file, int line, const char* fmt, ...) {
	char str[128];
	int offset = 0;

	/* Print prefix */
	offset += snprintf(str + offset, 128 - offset, "[");
	offset += snprintf_time(str + offset, 128 - offset);
	offset += snprintf(str + offset, 128 - offset, " %s%s%s %s:%d] ", log_level_color[level], log_level_str[level], COLOR_RESET, file_name_from_path(file), line);

	/* Print user string */
	va_list args;
	va_start(args, fmt);
	vsnprintf(str + offset, 128 - offset, fmt, args);
	va_end(args);

	serial_print(str);
}
