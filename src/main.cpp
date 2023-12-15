#include <avr/io.h>
#include <util/delay.h>
#include <arduino/USBAPI.h>
#include <stdio.h>

// from wiring.c
void init(void);
unsigned long millis(void);

int main(void) {
	init();

	DDRB = 0b00100000; // PB5 output

	Serial.begin(9600);

	Serial.print("Program Start\n");

	while (1) {
		PORTB = 0b00100000; // PB5
		_delay_ms(1000);

		Serial.print("Hello World\n");
		Serial.println(millis());

		PORTB = 0b00000000; // PB5
		_delay_ms(1000);
	}
}
