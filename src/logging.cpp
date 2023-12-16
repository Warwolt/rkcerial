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

static unsigned long try_get_timestamp(void) {
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

		if (string_starts_with(input_buf, "TIMENOW")) {
			int offset = strlen("TIMENOW ");
			timestamp_ms = strtol(&input_buf[offset], NULL, 10);
		}
	}

	return timestamp_ms;
}

static const char* COLOR_GREEN = "\033[32m";
static const char* COLOR_RESET = "\033[0m";

static void print_time_now() {
	char now_str[64] = { 0 };

	unsigned long now_ms = g_ms_since_midnight + millis();
	unsigned long hour = (now_ms / (1000L * 60L * 60L)) % 24L;
	unsigned long minutes = (now_ms / (1000L * 60L)) % 60L;
	unsigned long seconds = (now_ms / 1000L) % 60L;
	unsigned long milliseconds = now_ms % 1000L;

	snprintf(now_str, 64, "[%02lu:%02lu:%02lu:%03lu %sINFO%s %s:%d] ", hour, minutes, seconds, milliseconds, COLOR_GREEN, COLOR_RESET, "main.cpp", __LINE__);
	Serial.print(now_str);
}

#include <util/delay.h>


void rkcerial_init_logging(void) {
    init(); // wiring.c init, initializes the millis() function
    Serial.begin(9600);

    Serial.print("[ Logging ] Logging initialized, waiting for wall clock time time.\n");
    _delay_ms(2000); // FAKE WAITING
    Serial.print("[ Logging ] Timed out on getting wall clock time.\n");
}

void rkcerial_printf(const char* str) {
    print_time_now();
    Serial.print(str);
}
