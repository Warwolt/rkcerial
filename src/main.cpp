#include <avr/io.h>
#include <util/delay.h>
#include <arduino/USBAPI.h>

int main(void) {
	DDRB = 0b00100000; // PB5 output

	while (1) {
		PORTB = 0b00100000; // PB5
		_delay_ms(1000);

		PORTB = 0b00000000; // PB5
		_delay_ms(1000);
	}
}
