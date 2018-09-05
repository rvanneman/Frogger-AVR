/*
 * timer0.c
 *
 * Author: Peter Sutton Modified by: Becca Vanneman
 *
 * We setup timer0 to generate an interrupt every 1ms
 * We update a global clock tick variable - whose value
 * can be retrieved using the get_clock_ticks() function.
 */

#include <avr/io.h>
#include <avr/interrupt.h>

#include "timer0.h"

#define TOTAL_TIME 15000

// Seven segment 0-9
static uint8_t seven_seg[10] = {63, 6, 91, 79, 102, 109, 125, 7, 127, 111};

/* Our internal clock tick count - incremented every 
 * millisecond. Will overflow every ~49 days. */
static volatile uint32_t clockTicks;

// For frog countdown
static  uint8_t countdown_inited = 0;
static volatile uint8_t times_up = 0;
static volatile uint32_t frog_start_time;
static volatile uint8_t last_digit_shown = 0;

// Implement pause feature
static volatile uint32_t start_pause;
static volatile uint32_t total_time_paused = 0;
static volatile uint32_t paused_time = 0;
static uint8_t is_paused = 0;


/* Set up timer 0 to generate an interrupt every 1ms. 
 * We will divide the clock by 64 and count up to 124.
 * We will therefore get an interrupt every 64 x 125
 * clock cycles, i.e. every 1 milliseconds with an 8MHz
 * clock. 
 * The counter will be reset to 0 when it reaches its
 * output compare value.
 */
void init_timer0(void) {
	/* Reset clock tick count. L indicates a long (32 bit) 
	 * constant. 
	 */
	clockTicks = 0L;
	
	/* Clear the timer */
	TCNT0 = 0;

	/* Set the output compare value to be 124 */
	OCR0A = 124;
	
	/* Set the timer to clear on compare match (CTC mode)
	 * and to divide the clock by 64. This starts the timer
	 * running.
	 */
	TCCR0A = (1<<WGM01);
	TCCR0B = (1<<CS01)|(1<<CS00);

	/* Enable an interrupt on output compare match. 
	 * Note that interrupts have to be enabled globally
	 * before the interrupts will fire.
	 */
	TIMSK0 |= (1<<OCIE0A);
	
	/* Make sure the interrupt flag is cleared by writing a 
	 * 1 to it.
	 */
	TIFR0 &= (1<<OCF0A);
	
	// Countdown for frog
	DDRC = 0XFF;
	DDRD = (1<<2);
}

uint32_t get_current_time(void) {
	uint32_t returnValue;

	/* Disable interrupts so we can be sure that the interrupt
	 * doesn't fire when we've copied just a couple of bytes
	 * of the value. Interrupts are re-enabled if they were
	 * enabled at the start.
	 */
	uint8_t interruptsOn = bit_is_set(SREG, SREG_I);
	cli();
	returnValue = clockTicks;
	if(interruptsOn) {
		sei();
	}
	return returnValue;
}

ISR(TIMER0_COMPA_vect) {
	/* Increment our clock tick count */
	clockTicks++;
	
	paused_time = amount_time_paused();
	
	// Find time remaining
	uint32_t time_remaining = TOTAL_TIME - (get_current_time() - paused_time - frog_start_time);
	
	if (countdown_inited && time_remaining <= 0) {
		times_up = 1;
		PORTC = 0;
		PORTD &= ~(1<<2);
		PORTC = seven_seg[0];
		countdown_inited = 0;
	} else if (countdown_inited && last_digit_shown == 0) {
		if (time_remaining < 10000 && time_remaining >= 1000) {
			// Already correctly set
			PORTC = seven_seg[time_remaining/1000];
		} else if (time_remaining <= 15000 && time_remaining >= 10000){
	
			PORTD |= (1<<2); // Show leftmost digit
			PORTC = seven_seg[1];
			last_digit_shown = 1;
		} else if (time_remaining < 1000 && time_remaining > 0) {
			PORTD |= (1<<2); // Show leftmost digit
			PORTC = 191;
			//PORTC = seven_seg[0]; // Will always be 0
			//PORTC = (1<<7); // Show decimal point
			last_digit_shown = 1;
		}
	} else if (countdown_inited && last_digit_shown == 1) {
		if (time_remaining <= 15000 && time_remaining >= 10000){
			// 10-15 seconds
			PORTC = 0;
			PORTD &= ~(1<<2); // Make value 0
			PORTC = seven_seg[(time_remaining/1000)%10];
			last_digit_shown = 0;
		} else if (time_remaining < 1000 && time_remaining > 0) {
			PORTC = 0;
			PORTD &= ~(1<<2); // Show rightmost digit
			PORTC = seven_seg[time_remaining/100];
			last_digit_shown = 0;
		}
	}
}

void init_countdown() {
	//start timer
	times_up = 0;
	countdown_inited = 1;
	last_digit_shown = 0;
	total_time_paused = 0;
	frog_start_time = get_current_time();
}

uint8_t is_time_up(){
	return times_up;
}

void countdown_pause(){
	if (is_paused == 0) {
		start_pause = get_current_time();
		is_paused = 1;
	} else {
		total_time_paused += get_current_time() - start_pause;
		is_paused = 0;
	}
}

uint32_t amount_time_paused(void){
	uint32_t return_value = 0;
	if (is_paused) {
		return_value = total_time_paused + (get_current_time() - start_pause);
		return return_value;
	} else {
		// If not paused, only change by how
		// much has been paused by in past
		return total_time_paused;
	}
	
}