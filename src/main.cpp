#include <arduino/USBAPI.h>
#include <avr/io.h>
#include <stdio.h>
#include <util/delay.h>

// from wiring.c
void init(void);
unsigned long millis(void);

int main(void) {
	init();

	DDRB = 0b00100000; // PB5 output

	Serial.begin(9600);

	Serial.print("Program Start\n");

	char buffer[64] = { 0 };
	int buffer_len = 0;
	while (1) {
		while (Serial.available() > 0 && buffer_len < (64 - 2)) {
			buffer[buffer_len++] = Serial.read();
		}

		if (buffer_len > 0) {
			buffer[buffer_len++] = '\n';
			buffer[buffer_len++] = '\0';
			Serial.print(buffer);
			buffer_len = 0;
		}

		PORTB = 0b00100000; // PB5
		_delay_ms(1000);

		Serial.print("Hello World\n");
		Serial.println(millis());

		PORTB = 0b00000000; // PB5
		_delay_ms(1000);
	}
}
