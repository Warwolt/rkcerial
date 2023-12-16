#include <avr/io.h>
#include <util/delay.h>

#include <rkcerial/logging.h>

#include <stdio.h>
// TODO:
// log_info(fmt, ...)
// log_warning(fmt, ...)
// log_error(fmt, ...)

int main(void) {
	rkcerial_init_logging();

	DDRB = 0b00100000; // set output mode for pin PB5

	LOG_INFO("Program Start\n");

	unsigned long timestamp_ms = 0;
	while (1) {
		PORTB = 0b00100000; // set PB5
		_delay_ms(1000);

		LOG_INFO("Hello World\n");

		PORTB = 0b00000000; // clear PB5
		_delay_ms(1000);
		printf("");
	}
}
