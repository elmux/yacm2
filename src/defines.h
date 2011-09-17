/**
 * @brief   Preprocessor definitions
 * @file    defines.h
 * @version 1.0
 * @authors	Toni Baumann (bauma12@bfh.ch), Ronny Stauffer (staur3@bfh.ch), Elmar Vonlanthen (vonle1@bfh.ch)
 * @date    May 20, 2011
 */

#ifndef DEFINES_H_
#define DEFINES_H_

/**
 * Define the BOARD to use (CARME or ORCHID)
 */
//#define CARME
#ifndef CARME
	#define ORCHID
#endif


/**
 * Define symbol for the boolean value 'true'.
 */
#define TRUE 1
/**
 * Define symbol for the boolean value 'false'.
 */
#define FALSE 0

/**
 * Define NULL pointer
 */
#ifndef NULL
 #define NULL ((void*)0)
#endif

/**
 * Define end of file value
 */
#define EOF (-1)

/**
 * Represents an availability (e.g. of coffee, water, milk, ...).
 */
typedef enum {
	notAvailable,
	available
} Availability;

/**
 * Represents a selected value
 */
typedef enum {
	notSelected,
	selected
} Selector;

/**
 * Represents the coffee maker state.
 */
typedef enum {
	machineState_off,
	machineState_initializing,
	machineState_idle,
	machineState_producing
} MachineState;

/**
 * Define some switches
 */
#define POWER_SWITCH 1<<0
#define MILK_SELECTOR_SWITCH 1<<1

// Old
#define INIT_COMMAND 1
#define OFF_COMMAND 2
#define ABORT_COMMAND 3

#define OK_RESULT 101
#define NOK_RESULT 201

#endif /* DEFINES_H_ */
