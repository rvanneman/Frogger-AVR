/*
 * FroggerProject.c
 *
 * Main file
 *
 * Author: Peter Sutton. Modified by Becca Vanneman
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <stdio.h>

#include "ledmatrix.h"
#include "scrolling_char_display.h"
#include "buttons.h"
#include "serialio.h"
#include "terminalio.h"
#include "score.h"
#include "timer0.h"
#include "game.h"

#define F_CPU 8000000L
#include <util/delay.h>

// Function prototypes - these are defined below (after main()) in the order
// given here
void initialise_hardware(void);
void splash_screen(void);
void new_game(void);
void play_game(void);
void handle_game_over(void);
void update_score(void);
void update_level(uint32_t level);
void pause_game();

static uint32_t time_paused = 0;
static uint32_t begin_pause;
static uint8_t paused = 0;

// For Auto Repeat
uint8_t project_last_button_state;
uint8_t possible_button_states[4] = {1, 2, 4, 8};
uint32_t frog_last_moved;


// ASCII code for Escape character
#define ESCAPE_CHAR 27

/////////////////////////////// main //////////////////////////////////
int main(void) {
	// Setup hardware and call backs. This will turn on 
	// interrupts.
	initialise_hardware();
	
	// Show the splash screen message. Returns when display
	// is complete
	splash_screen();
	
	while(1) {
		new_game();
		play_game();
		handle_game_over();
	}
}

void initialise_hardware(void) {
	ledmatrix_setup();
	init_button_interrupts();
	
	// Setup serial port for 19200 baud communication with no echo
	// of incoming characters
	init_serial_stdio(19200,0);
	
	init_timer0();
	
	// Turn on global interrupts
	sei();
}

void splash_screen(void) {
	// Clear terminal screen and output a message
	clear_terminal();
	move_cursor(10,10);
	printf_P(PSTR("Frogger by 46258839"));
	move_cursor(10,12);
	printf_P(PSTR("CSSE2010 project by Rebecca Vanneman"));
	
	// Output the scrolling message to the LED matrix
	// and wait for a push button to be pushed.
	ledmatrix_clear();
	while(1) {
		set_scrolling_display_text("FROGGER 45258839", COLOUR_GREEN);
		// Scroll the message until it has scrolled off the 
		// display or a button is pushed
		while(scroll_display()) {
			_delay_ms(150);
			if(button_pushed() != NO_BUTTON_PUSHED) {
				return;
			}
		}
	}
}

void new_game(void) {
	// Initialise the game and display
	initialise_game();
	
	// Clear the serial terminal
	clear_terminal();
	
	// Initialise the score and put score/level on serial terminal
	init_score();
	update_score();
	update_level(1);
	init_led();
	
	// Clear a button push or serial input if any are waiting
	// (The cast to void means the return value is ignored.)
	(void)button_pushed();
	clear_serial_input_buffer();
}

void play_game(void) {
	uint32_t current_time, last_move_time;
	uint8_t road0, road1, road2, river0, river1;
	road0 = road1 = road2 = river0 = river1 = 0;
	int8_t button;
	char serial_input, escape_sequence_char;
	uint8_t characters_into_escape_sequence = 0;
	uint32_t level = 1; // Specifies level
	uint16_t speed_up_by = 0; // Rates will speed up each level it increases
	
	// Start Countdown
	
	init_countdown();
	// Get the current time and remember this as the last time the vehicles
	// and logs were moved.
	current_time = get_current_time();
	last_move_time = current_time;
	
	// We play the game while the frog is alive and we haven't filled up the 
	// far riverbank
	while(!is_frog_dead()) {
		
		if (is_time_up()) {
			// Frog ran out of time
			decrement_lives();
			if (!is_frog_dead()){
				initialise_life(level);
				init_countdown();
			}
		}else if (!is_frog_dead() && is_riverbank_full()){
			// If the riverbank is full and frog isn't dead, start a new level
			// somehow use scroll_display to scroll level over -- check splash screen stuff
			scroll_display();
			level++;
			initialise_level(level);
			update_level(level);
			init_countdown();
			
			// Each level speeds up by 50 ms
			speed_up_by = speed_up_by + 50;
			
		}
		if(!is_frog_dead() && !is_decremented() && frog_has_reached_riverbank()) {
			// Frog reached the other side successfully but the
			// riverbank isn't full, put a new frog at the start
			put_frog_in_start_position();
			
			init_countdown();
		} else if (is_decremented() && !is_frog_dead()) {
			initialise_life(level);
			
			init_countdown();
		}
		
		// Check for input - which could be a button push or serial input.
		// Serial input may be part of an escape sequence, e.g. ESC [ D
		// is a left cursor key press. At most one of the following three
		// variables will be set to a value other than -1 if input is available.
		// (We don't initalise button to -1 since button_pushed() will return -1
		// if no button pushes are waiting to be returned.)
		// Button pushes take priority over serial input. If there are both then
		// we'll retrieve the serial input the next time through this loop
		serial_input = -1;
		escape_sequence_char = -1;
		button = button_pushed();
		
		if(button == NO_BUTTON_PUSHED) {
			// No push button was pushed, see if there is any serial input
			if(serial_input_available()) {
				// Serial data was available - read the data from standard input
				serial_input = fgetc(stdin);
				// Check if the character is part of an escape sequence
				if(characters_into_escape_sequence == 0 && serial_input == ESCAPE_CHAR) {
					// We've hit the first character in an escape sequence (escape)
					characters_into_escape_sequence++;
					serial_input = -1; // Don't further process this character
				} else if(characters_into_escape_sequence == 1 && serial_input == '[') {
					// We've hit the second character in an escape sequence
					characters_into_escape_sequence++;
					serial_input = -1; // Don't further process this character
				} else if(characters_into_escape_sequence == 2) {
					// Third (and last) character in the escape sequence
					escape_sequence_char = serial_input;
					serial_input = -1;  // Don't further process this character - we
										// deal with it as part of the escape sequence
					characters_into_escape_sequence = 0;
				} else {
					// Character was not part of an escape sequence (or we received
					// an invalid second character in the sequence). We'll process 
					// the data in the serial_input variable.
					characters_into_escape_sequence = 0;
				}
			}
		}
		
		project_last_button_state = show_button_state();
		// Process the input. 
		if(!paused && (button==3 || escape_sequence_char=='D' || serial_input=='L' || serial_input=='l')) {
			// Attempt to move left
			move_frog_to_left();
			frog_last_moved = get_current_time();
			project_last_button_state = 0;
		} else if(!paused && (button==2 || escape_sequence_char=='A' || serial_input=='U' || serial_input=='u')) {
			// Attempt to move forward
			move_frog_forward();
			frog_last_moved = get_current_time();
			project_last_button_state = 0;
			update_score();
			update_level(level);
		} else if(!paused && (button==1 || escape_sequence_char=='B' || serial_input=='D' || serial_input=='d')) {
			// Attempt to move down
			move_frog_backward();
			frog_last_moved = get_current_time();
			project_last_button_state = 0;
		} else if(!paused && (button==0 || escape_sequence_char=='C' || serial_input=='R' || serial_input=='r')) {
			// Attempt to move right
			move_frog_to_right();
			frog_last_moved = get_current_time();
			project_last_button_state = 0;
		} else if(serial_input == 'p' || serial_input == 'P') {
			pause_game();
		}  else if (!paused && ((get_current_time() - 300) >= frog_last_moved)) {

			if (project_last_button_state == possible_button_states[0]){
				move_frog_to_right();	
			} else if (project_last_button_state == possible_button_states[1]){
				move_frog_backward();
			} else if(project_last_button_state == possible_button_states[2]){
				move_frog_forward();
			} else if (project_last_button_state == possible_button_states[3]){
				move_frog_to_left();
			}
			frog_last_moved = get_current_time();
			project_last_button_state = 0;
		// No other if statements added because if multiple buttons pushed
		// We ignore both buttons
		}
		
			

		current_time = get_current_time();
		if(!paused && !is_frog_dead() && !road2 && current_time - time_paused >= last_move_time + 750 - speed_up_by) {
			// 1000ms (1 second) has passed since the last time we moved
			// the vehicles and logs - move them again and keep track of
			// the time when we did this. 
			scroll_vehicle_lane(2, 1);
			road2 = 1;
		} 	
		if(!paused && !is_frog_dead() && !river0 && current_time - time_paused >= last_move_time + 850 - speed_up_by) {
			scroll_river_channel(0, -1);
			river0 = 1;

		}
		if(!paused && !is_frog_dead() && !road0 && current_time - time_paused >= last_move_time + 1000 - speed_up_by) {
			scroll_vehicle_lane(0, 1);
			road0 = 1;
		}
		if(!paused && !is_frog_dead() && !river1 && current_time - time_paused >= last_move_time + 1200 - speed_up_by) {
			scroll_river_channel(1, 1);
			river1 = 1;
		}
		if(!paused && !is_frog_dead() && !road1 && current_time - time_paused >= last_move_time + 1300 -speed_up_by) {
			scroll_vehicle_lane(1, -1);
			last_move_time = current_time;
			road0 = road1 = road2 = river0 = river1 = 0;
			time_paused = 0;
		}
	}
	// We get here if the frog is dead.
	// The game is over.
}


void handle_game_over() {
	move_cursor(10,14);
	printf_P(PSTR("GAME OVER"));
	move_cursor(10,15);
	printf_P(PSTR("Press a button to start again"));
	while(button_pushed() == NO_BUTTON_PUSHED) {
		; // wait
	}
	
}

void update_score() {
	clear_terminal();
	move_cursor(30,2);
	//printf_P(PSTR("\x1b[K"));
	printf_P(PSTR("Score:"));
	
	int score = get_score();
	if (score < 10) {
		move_cursor(40,2);
	} else if (score >10 && score < 100) {
		move_cursor(39,2);
	} else if (score > 100 && score < 1000){
		move_cursor(38,2);
	} else if (score > 1000 && score < 10000){
		move_cursor(37,2);
	}
	printf_P(PSTR("%d"), score);
}

void update_level(uint32_t level) {
	move_cursor(30,3);
	printf_P(PSTR("Level %d"), level);
}

void pause_game() {
	// Game is already paused, unpause
	if (paused == 1){
		uint32_t now = get_current_time();
		time_paused = now - begin_pause;
		paused = 0;
		countdown_pause();
	} else {
		// Pause Game
		begin_pause = get_current_time();
		paused = 1;
		countdown_pause();
	}
}




