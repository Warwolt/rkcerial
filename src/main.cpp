#include <avr/io.h>
#include <util/delay.h>
#include <arduino/USBAPI.h>
#include <stdio.h>

int main(void) {
	DDRB = 0b00100000; // PB5 output

	Serial.begin(9600);

	int num = 0;

	while (1) {
		PORTB = 0b00100000; // PB5
		_delay_ms(1000);

		Serial.print(num);

		PORTB = 0b00000000; // PB5
		_delay_ms(1000);
	}
}
