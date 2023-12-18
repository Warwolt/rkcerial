#include <avr/io.h>
#include <util/delay.h>

#include <rkcerial/logging.h>

int main(void) {
	rk_init_logging();

	LOG_INFO("Program Start\n");
	LOG_INFO("Info\n");
	LOG_WARNING("Warning\n");
	LOG_ERROR("Error\n");

	while (1) {
		LOG_INFO("Hello World\n");
		_delay_ms(1000);
	}
}
