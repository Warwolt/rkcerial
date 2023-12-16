#include <rkcerial/logging.h>

#include <arduino/USBAPI.h>
#include <avr/io.h>
#include <stdio.h>

// from wiring.c
void init(void);
unsigned long millis(void);

static unsigned long g_ms_since_midnight = 0;

static bool string_starts_with(const char* str, const char* prefix) {
	return strncmp(prefix, str, strlen(prefix)) == 0;
}

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

static const char* COLOR_GREEN = "\033[32m";
static const char* COLOR_RESET = "\033[0m";

static int print_time(char* str_buf, size_t str_buf_len) {
	unsigned long now_ms = g_ms_since_midnight + millis();
	unsigned long hour = (now_ms / (1000L * 60L * 60L)) % 24L;
	unsigned long minutes = (now_ms / (1000L * 60L)) % 60L;
	unsigned long seconds = (now_ms / 1000L) % 60L;
	unsigned long milliseconds = now_ms % 1000L;

	return snprintf(str_buf, str_buf_len, "%02lu:%02lu:%02lu:%03lu", hour, minutes, seconds, milliseconds);
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

void rkcerial_init_logging(void) {
	init(); // wiring.c init, initializes the millis() function
	Serial.begin(9600);

	rkcerial_printf("[ Logging ] Logging initialized, waiting for wall clock time.\n");

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
			rkcerial_printf("[ Logging ] Received wall clock time.\n", g_ms_since_midnight);
			break;
		}

		/* Timed out, abort */
		const unsigned long now_ms = millis();
		if (now_ms - start_ms > timeout_ms) {
			rkcerial_printf("[ Logging ] Timed out on getting wall clock time (waited %lu milliseconds).\n", timeout_ms);
			break;
		}
	}
}

void rkcerial_printf(const char* fmt, ...) {
	char str[128];
	va_list args;
	va_start(args, fmt);
	vsnprintf(str, 128, fmt, args);
	va_end(args);

	Serial.print(str);
}

void rkcerial_log_info(const char* file, int line, const char* fmt, ...) {
	char str[128];
	int offset = 0;

	/* Prefix */
	offset += snprintf(str + offset, 128 - offset, "[");
	offset += print_time(str + offset, 128 - offset);
	offset += snprintf(str + offset, 128 - offset, " INFO %s:%d] ", file, line);

	/* Print user string */
	va_list args;
	va_start(args, fmt);
	vsnprintf(str + offset, 128 - offset, fmt, args);
	va_end(args);

	Serial.print(str);
}
