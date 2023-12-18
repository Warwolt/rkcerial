#include <avr/io.h>
#include <util/delay.h>

#include <rkcerial/logging.h>

int main(void) {
	DDRB = 0b00100000; // set output mode for pin PB5
	PORTB = 0b00000000; // clear PB5

	rk_init_logging();

	LOG_INFO("Program Start\n");

	LOG_INFO("Info\n");
	LOG_WARNING("Warning\n");
	LOG_ERROR("Error\n");

	unsigned long timestamp_ms = 0;
	while (1) {
		// PORTB = 0b00100000; // set PB5
		_delay_ms(1000);

		LOG_INFO("Hello World\n");

		// PORTB = 0b00000000; // clear PB5
		// _delay_ms(1000);
	}
}
