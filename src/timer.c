#include "timer.h"

#include <avr/io.h>
#include <util/atomic.h>

#define clear_bit(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#define set_bit(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))

#define CLOCK_CYCLES_PER_MICROSECOND() (F_CPU / 1000000L)
#define CLOCK_CYCLES_TO_MICROSECONDS(a) ((a) / CLOCK_CYCLES_PER_MICROSECOND())

// the prescaler is set so that timer0 ticks every 64 clock cycles, and the
// the overflow handler is called every 256 ticks.
#define MICROSECONDS_PER_TIMER0_OVERFLOW (CLOCK_CYCLES_TO_MICROSECONDS(64 * 256))

// the whole number of milliseconds per timer0 overflow
#define MILLIS_INC (MICROSECONDS_PER_TIMER0_OVERFLOW / 1000)

// the fractional number of milliseconds per timer0 overflow. we shift right
// by three to fit these numbers into a byte. (for the clock speeds we care
// about - 8 and 16 MHz - this doesn't lose precision.)
#define FRACT_INC ((MICROSECONDS_PER_TIMER0_OVERFLOW % 1000) >> 3)
#define FRACT_MAX (1000 >> 3)

static volatile unsigned long g_timer0_overflow_count = 0;
static volatile unsigned long g_timer0_millis = 0;
static unsigned char g_timer0_fract = 0;

ISR(TIMER0_OVF_vect) {
	// copy these to local variables so they can be stored in registers
	// (volatile variables must be read from memory on every access)
	unsigned long m = g_timer0_millis;
	unsigned char f = g_timer0_fract;

	m += MILLIS_INC;
	f += FRACT_INC;
	if (f >= FRACT_MAX) {
		f -= FRACT_MAX;
		m += 1;
	}

	g_timer0_fract = f;
	g_timer0_millis = m;
	g_timer0_overflow_count++;
}

// Configures Timer 0 to be used for counting elapsed milliseconds
void timer_initialize() {
	// Set prescale factor to be 64
	set_bit(TCCR0B, CS01);
	set_bit(TCCR0B, CS00);

	// Enable timer 0 overflow interrupt
	set_bit(TIMSK0, TOIE0);
}

// returns num elapsed milliseconds since program start
unsigned long time_now_ms() {
	unsigned long now_ms;
	uint8_t old_SREG = SREG;

	// disable interrupts while we read g_timer0_millis or we might get an
	// inconsistent value (e.g. in the middle of a write to g_timer0_millis)
	cli();
	now_ms = g_timer0_millis;
	SREG = old_SREG;

	return now_ms;
}
