#include <rkcerial/logging.h>

#include <arduino/USBAPI.h>
#include <avr/io.h>
#include <stdio.h>

// from wiring.c
void init(void);
unsigned long millis(void);

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
// Serial::begin
#define SERIAL_8N1 0x06

#define clear_bit(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define set_bit(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))

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

static int read_byte(void) {
	const unsigned long timeout_ms = 1000;
	unsigned long start_ms = millis();
	int byte;
	do {
		byte = Serial.read();
		if (byte >= 0) {
			return byte;
		}
	} while (millis() - start_ms < timeout_ms);
	return -1; // timed out
}

static void read_string(char* str_buf, size_t str_buf_len) {
	int index = 0;
	int byte = read_byte();
	while (byte >= 0 && index < str_buf_len) {
		str_buf[index++] = (char)byte;
		byte = read_byte();
	}
}

/* ------------------------------- Time stamp ------------------------------- */
static unsigned long try_get_ms_since_midnight(void) {
	unsigned long timestamp_ms = 0;

	char input_buf[64] = { 0 };
	int input_buf_len = 0;
	while (Serial.available() > 0 && input_buf_len < (64 - 2)) {
		input_buf[input_buf_len++] = Serial.read();
	}

	if (input_buf_len > 0) {
		input_buf[input_buf_len++] = '\n';
		input_buf[input_buf_len++] = '\0';
		input_buf_len = 0;

		Serial.print(input_buf);

		if (string_starts_with(input_buf, "TIMENOW")) {
			Serial.print("Got TIMENOW\n");
			int offset = strlen("TIMENOW ");
			timestamp_ms = strtol(&input_buf[offset], NULL, 10);
		}
	}

	return timestamp_ms;
}

static int sprintf_time(char* str_buf, size_t str_buf_len) {
	unsigned long now_ms = g_ms_since_midnight + millis();
	unsigned long hour = (now_ms / (1000L * 60L * 60L)) % 24L;
	unsigned long minutes = (now_ms / (1000L * 60L)) % 60L;
	unsigned long seconds = (now_ms / 1000L) % 60L;
	unsigned long milliseconds = now_ms % 1000L;

	return snprintf(str_buf, str_buf_len, "%02lu:%02lu:%02lu:%03lu", hour, minutes, seconds, milliseconds);
}

/* ------------------------------- Public API ------------------------------- */
void rk_init_logging(void) {
	init(); // wiring.c init, initializes the millis() function
	serial_initialize(9600);

	rk_printf("[ Logging ] Logging initialized, waiting for wall clock time.\n");

	char input_buf[64] = { 0 };
	const unsigned long timeout_ms = 2000;
	const unsigned long start_ms = millis();
	while (true) {
		/* Read input, look for clock time */
		read_string(input_buf, 64);
		if (string_starts_with(input_buf, "TIMENOW")) {
			/* Received clock time */
			int offset = strlen("TIMENOW ");
			g_ms_since_midnight = strtol(input_buf + offset, NULL, 10);
			rk_printf("[ Logging ] Received wall clock time.\n", g_ms_since_midnight);
			break;
		}

		/* Timed out, abort */
		const unsigned long now_ms = millis();
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

	Serial.print(str);
}

void rk_log(rk_log_level_t level, const char* file, int line, const char* fmt, ...) {
	char str[128];
	int offset = 0;

	/* Print prefix */
	offset += snprintf(str + offset, 128 - offset, "[");
	offset += sprintf_time(str + offset, 128 - offset);
	offset += snprintf(str + offset, 128 - offset, " %s%s%s %s:%d] ", log_level_color[level], log_level_str[level], COLOR_RESET, file_name_from_path(file), line);

	/* Print user string */
	va_list args;
	va_start(args, fmt);
	vsnprintf(str + offset, 128 - offset, fmt, args);
	va_end(args);

	Serial.print(str);
}
