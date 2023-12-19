#include "timer.h"
#include <rkcerial/logging.h>

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>

void globally_enable_interrupts() {
	sei();
}

int main(void) {
	globally_enable_interrupts();
	timer_initialize();
	rk_init_logging(&time_now_ms);

	LOG_INFO("Program Start\n");
	LOG_INFO("Info\n");
	LOG_WARNING("Warning\n");
	LOG_ERROR("Error\n");

	while (1) {
		LOG_INFO("Hello World\n");
		_delay_ms(1000);
	}
}
