#include <arduino/USBAPI.h>
#include <avr/io.h>
#include <stdio.h>
#include <util/delay.h>

// from wiring.c
void init(void);
unsigned long millis(void);

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

static void print_time_now(unsigned long timestamp_ms) {
	char now_str[64] = { 0 };

	unsigned long now_ms = timestamp_ms + millis();

	unsigned long hour = (now_ms / (1000L * 60L * 60L)) % 24L;
	unsigned long minutes = (now_ms / (1000L * 60L)) % 60L;
	unsigned long seconds = (now_ms / 1000L) % 60L;
	unsigned long milliseconds = now_ms % 1000L;
	snprintf(now_str, 64, "[%02lu:%02lu:%02lu:%02lu] ", hour, minutes, seconds, milliseconds);

	Serial.print(now_str);
}

int main(void) {
	init();

	DDRB = 0b00100000; // PB5 output

	Serial.begin(9600);
	Serial.print("Program Start\n");

	unsigned long timestamp_ms = 0;
	while (1) {
		if (timestamp_ms == 0) {
			timestamp_ms = try_get_timestamp();
		}

		PORTB = 0b00100000; // PB5
		_delay_ms(1000);

		if (timestamp_ms > 0) {
			print_time_now(timestamp_ms);
		}
		Serial.print("Hello World\n");

		PORTB = 0b00000000; // PB5
		_delay_ms(1000);
	}
}
