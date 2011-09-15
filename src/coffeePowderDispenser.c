/**
 * @brief   Submodule coffee powder dispenser
 * @file    coffeePowderDispenser.c
 * @version 1.0
 * @authors	Toni Baumann (bauma12@bfh.ch), Ronny Stauffer (staur3@bfh.ch), Elmar Vonlanthen (vonle1@bfh.ch)
 * @date    Aug 15, 2011
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "defines.h"
#include "syslog.h"
#include "device.h"
#include "mainController.h"
#include "coffeePowderDispenser.h"
#include "coffeeSupply.h"
#include "activity.h"
#include "stateMachineEngine.h"

static void setUpCoffeePowderDispenser(void *activity);
static void runCoffeePowderDispenser(void *activity);
static void tearDownCoffeePowderDispenser(void *activity);
static void setUpFillStateMonitor(void *activity);
static void runFillStateMonitor(void *activity);
static void tearDownFillStateMonitor(void *activity);
static void setUpMotorController(void *activity);
static void runMotorController(void *activity);
static void tearDownMotorController(void *activity);

static StateMachine coffeePowderDispenserStateMachine;

static Activity *coffeePowderDispenser;
static Activity *fillStateMonitor;
static Activity *motorController;

static ActivityDescriptor coffeePowderDispenserDescriptor = {
	.name = "coffeePowderDispenser",
	.setUp = setUpCoffeePowderDispenser,
	.run = runCoffeePowderDispenser,
	.tearDown = tearDownCoffeePowderDispenser
};

static ActivityDescriptor fillStateMonitorDescriptor = {
	.name = "fillStateMonitor",
	.setUp = setUpFillStateMonitor,
	.run = runFillStateMonitor,
	.tearDown = tearDownFillStateMonitor
};

static ActivityDescriptor motorControllerDescriptor = {
	.name = "motorController",
	.setUp = setUpMotorController,
	.run = runMotorController,
	.tearDown = tearDownMotorController
};

static int setMotor(int power) {
	char level[2];
	if (power > 99) power = 99;
	if (power < 0) power = 0;
	snprintf(level,2,"%d",power);
	return writeNonBlockingDevice("/dev/coffeeGrinderMotor",level,wrm_replace,FALSE);
}

static int hasEnoughPowder(void) {
	return readNonBlockingDevice("./dev/coffeePowderDispenser");
}

static int hasBeans(void) {
	return readNonBlockingDevice("./dev/coffeeBeansSensor");
}

static int lastHasBeansState = FALSE;

static int checkBeans(void) {
	int hasBeansState = hasBeans();
	if (hasBeansState != lastHasBeansState) {
		logInfo("[fillStateMonitor] Beans state changed to %d",hasBeansState);
		lastHasBeansState = hasBeansState;

		if (hasBeansState) {
			sendMessage(getCoffeePowderDispenser(), (char *)&(FillStateMonitorMessage) {
				.activity = getFillStateMonitor(),
				.intValue = POWDER_DISPENSER_BEANS_AVAILABLE_NOTIFICATION,
			}, sizeof(FillStateMonitorMessage), messagePriority_high);
		} else {
			sendMessage(getCoffeePowderDispenser(), (char *)&(FillStateMonitorMessage) {
				.activity = getFillStateMonitor(),
				.intValue = POWDER_DISPENSER_NO_BEANS_ERROR,
			}, sizeof(FillStateMonitorMessage), messagePriority_high);
		}
	}

	return hasBeansState;
}


/*
 ***************************************************************************
 * States coffeePowderDispenser
 ***************************************************************************
 */

// off:
// initializing:
// idle: bereit;
// supplying;
/**
 * Represents a coffee powder dispenser state
 */
typedef enum {
	coffeePowderDispenserState_switchedOff,
	coffeePowderDispenserState_initializing,
	coffeePowderDispenserState_idle,
	coffeePowderDispenserState_supplying
} CoffeePowderDispenserState;

/*
 ***************************************************************************
 * switchedOff state powder dispenser
 ***************************************************************************
 */

static void coffeePowderDispenserSwitchedOffStateEntryAction() {
	logInfo("[coffeePowderDispenser] Entered SwitchedOff State...");
}

static Event coffeePowderDispenserSwitchedOffStateDoAction() {
	return NO_EVENT;
}

static State coffeePowderDispenserSwitchedOffState = {
	.stateIndex = coffeePowderDispenserState_switchedOff,
	.entryAction = coffeePowderDispenserSwitchedOffStateEntryAction,
	.doAction = coffeePowderDispenserSwitchedOffStateDoAction
};


/*
 ***************************************************************************
 * initializing state powder dispenser
 ***************************************************************************
 */

static void coffeePowderDispenserInitializingStateEntryAction() {
	// notifiy motorController:
	sendMessage(getMotorController(), (char *)&(MotorControllerMessage) {
		.activity = getCoffeePowderDispenser(),
		.intValue = MOTOR_STOP_COMMAND,
		.strValue = "stop motor",
	}, sizeof(MotorControllerMessage), messagePriority_medium);
}

static Event coffeePowderDispenserInitializingStateDoAction() {
	sleep(1);
	return coffeePowderDispenserEvent_initialized;
}


static State coffeePowderDispenserInitializingState = {
	.stateIndex = coffeePowderDispenserState_initializing,
	.entryAction = coffeePowderDispenserInitializingStateEntryAction,
	.doAction = coffeePowderDispenserInitializingStateDoAction
};


/*
 ***************************************************************************
 * idle state powder dispenser
 ***************************************************************************
 */

static void coffeePowderDispenserIdleStateEntryAction() {
	;
}

static Event coffeePowderDispenserIdleStateDoAction() {

	return NO_EVENT;
}

static State coffeePowderDispenserIdleState = {
	.stateIndex = coffeePowderDispenserState_idle,
	.entryAction = coffeePowderDispenserIdleStateEntryAction,
	.doAction = coffeePowderDispenserIdleStateDoAction
};

/*
 ***************************************************************************
 * supplying state powder dispenser
 ***************************************************************************
 */

static void coffeePowderDispenserSupplyingStateEntryAction() {
	// notifiy motorController:
	sendMessage(getMotorController(), (char *)&(MotorControllerMessage) {
		.activity = getCoffeePowderDispenser(),
		.intValue = MOTOR_START_COMMAND,
		.strValue = "start motor",
	}, sizeof(MotorControllerMessage), messagePriority_medium);
}

static Event coffeePowderDispenserSupplyingStateDoAction() {
	return NO_EVENT;
}

static void coffeePowderDispenserSupplyingStateExitAction() {
	// notifiy motorController:
	sendMessage(getMotorController(), (char *)&(MotorControllerMessage) {
		.activity = getCoffeePowderDispenser(),
		.intValue = MOTOR_STOP_COMMAND,
		.strValue = "stop motor",
	}, sizeof(MotorControllerMessage), messagePriority_medium);
	// notifiy coffeeSupply:
	sendMessage(getCoffeeSupplyDescriptor(), (char *)&(CoffeeSupplyMessage) {
		.activity = getCoffeePowderDispenser(),
		.intValue = OK_RESULT,
		.strValue = "grinding complete",
	}, sizeof(CoffeeSupplyMessage), messagePriority_medium);
}

static State coffeePowderDispenserSupplyingState = {
	.stateIndex = coffeePowderDispenserState_supplying,
	.entryAction = coffeePowderDispenserSupplyingStateEntryAction,
	.doAction = coffeePowderDispenserSupplyingStateDoAction,
	.exitAction = coffeePowderDispenserSupplyingStateExitAction
};


/*
 ***************************************************************************
 * State transitions powder dispenser
 ***************************************************************************
 */

static StateMachine coffeePowderDispenserStateMachine = {
	.numberOfEvents = 8,
	.initialState = &coffeePowderDispenserSwitchedOffState,
	.transitions = {
		/* coffeePowderDispenserState_switchedOff: */
			/* coffeePowderDispenserEvent_init: */ &coffeePowderDispenserInitializingState,
			/* coffeePowderDispenserEvent_switchOff: */ NULL,
			/* coffeePowderDispenserEvent_initialized: */ NULL,
			/* coffeePowderDispenservent_startSupplying: */ NULL,
			/* coffeePowderDispenserEvent_supplyingFinished: */ NULL,
			/* coffeePowderDispenserEvent_stop: */ NULL,
			/* coffeePowderDispenserEvent_noBeans: */ NULL,
			/* coffeePowderDispenserEvent_beansAvailable: */ NULL,
		/* coffeePowderDispenserState_initializing: */
			/* coffeePowderDispenserEvent_init: */ NULL,
			/* coffeePowderDispenserEvent_switchOff: */ &coffeePowderDispenserSwitchedOffState,
			/* coffeePowderDispenserEvent_initialized: */ &coffeePowderDispenserIdleState,
			/* coffeePowderDispenserEvent_startSupplying: */ NULL,
			/* coffeePowderDispenserEvent_supplyingFinished: */ NULL,
			/* coffeePowderDispenserEvent_stop: */ NULL,
			/* coffeePowderDispenserEvent_noBeans: */ NULL,
			/* coffeePowderDispenserEvent_beansAvailable: */ NULL,
		/* coffeePowderDispenserState_idle: */
			/* coffeePowderDispenserEvent_init: */ &coffeePowderDispenserInitializingState,
			/* coffeePowderDispenserEvent_switchOff: */ &coffeePowderDispenserSwitchedOffState,
			/* coffeePowderDispenserEvent_initialized: */ NULL,
			/* coffeePowderDispenserEvent_startSupplying: */ &coffeePowderDispenserSupplyingState,
			/* coffeePowderDispenserEvent_supplyingFinished: */ NULL,
			/* coffeePowderDispenserEvent_stop: */ NULL,
			/* coffeePowderDispenserEvent_noBeans: */ NULL,
			/* coffeePowderDispenserEvent_beansAvailable: */ NULL,
		/* coffeePowderDispenserState_supplying: */
			/* coffeePowderDispenserEvent_init: */ NULL,
			/* coffeePowderDispenserEvent_switchOff: */ &coffeePowderDispenserSwitchedOffState,
			/* coffeePowderDispenserEvent_initialized: */ NULL,
			/* coffeePowderDispenserEvent_startSupplying: */ NULL,
			/* coffeePowderDispenserEvent_supplyingFinished: */ &coffeePowderDispenserIdleState,
			/* coffeePowderDispenserEvent_stop: */ &coffeePowderDispenserIdleState,
			/* coffeePowderDispenserEvent_noBeans: */ &coffeePowderDispenserIdleState,
			/* coffeePowderDispenserEvent_beansAvailable: */ NULL
		}
};

ActivityDescriptor getCoffeePowderDispenser() {
	return coffeePowderDispenserDescriptor;
}

ActivityDescriptor getFillStateMonitor() {
	return fillStateMonitorDescriptor;
}

ActivityDescriptor getMotorController() {
	return motorControllerDescriptor;
}

static void setUpCoffeePowderDispenser(void *activityarg) {
	logInfo("[coffeePowderDispenser] Setting up...");
	coffeePowderDispenser = activityarg;
	setUpStateMachine(&coffeePowderDispenserStateMachine);
	fillStateMonitor = createActivity(getFillStateMonitor(), messageQueue_blocking);
	motorController = createActivity(getMotorController(), messageQueue_blocking);
}

static void runCoffeePowderDispenser(void *activity) {
	logInfo("[coffeePowderDispenser] Running...");
	while (TRUE) {
		// Wait for incoming message or time event
		CoffeePowderDispenserMessage incomingMessage;
		int result = waitForEvent(coffeePowderDispenser, (char *)&incomingMessage, sizeof(incomingMessage), 100);
		if (result < 0) {
			//TODO Implement apropriate error handling
			sleep(10);
			// Try to recover from error
			continue;
		}

		// Check if there is an incoming message
		if (result > 0) {
			// Process incoming message
			switch (incomingMessage.intValue) {
				case INIT_COMMAND:
					processStateMachineEvent(&coffeePowderDispenserStateMachine, coffeePowderDispenserEvent_init);
					break;
				case OFF_COMMAND:
					processStateMachineEvent(&coffeePowderDispenserStateMachine, coffeePowderDispenserEvent_switchOff);
					break;
				case POWDER_DISPENSER_START_COMMAND:
					if (lastHasBeansState) {
						processStateMachineEvent(&coffeePowderDispenserStateMachine, coffeePowderDispenserEvent_startSupplying);
					}
					break;
				case POWDER_DISPENSER_STOP_COMMAND:
					processStateMachineEvent(&coffeePowderDispenserStateMachine, coffeePowderDispenserEvent_stop);
					break;
				case POWDER_DISPENSER_NO_BEANS_ERROR:
					processStateMachineEvent(&coffeePowderDispenserStateMachine, coffeePowderDispenserEvent_noBeans);
					sendMessage(getCoffeeSupplyDescriptor(),(char *)&(CoffeeSupplyMessage){
						.activity = getCoffeePowderDispenser(),
						.intValue = SUPPLY_NO_BEANS_ERROR,
						.strValue = "No beans"
						}, sizeof(CoffeeSupplyMessage), messagePriority_medium);
					break;
				case POWDER_DISPENSER_BEANS_AVAILABLE_NOTIFICATION:
					processStateMachineEvent(&coffeePowderDispenserStateMachine, coffeePowderDispenserEvent_beansAvailable);
					sendMessage(getCoffeeSupplyDescriptor(),(char *)&(CoffeeSupplyMessage){
						.activity = getCoffeePowderDispenser(),
						.intValue = SUPPLY_BEANS_AVAILABLE_NOTIFICATION,
						.strValue = "Beans available"
						}, sizeof(CoffeeSupplyMessage), messagePriority_medium);
					break;
			}
		}
	}
	// Run state machine
	runStateMachine(&coffeePowderDispenserStateMachine);
	// enough Powder
	if (hasEnoughPowder()) {
		processStateMachineEvent(&coffeePowderDispenserStateMachine, coffeePowderDispenserEvent_supplyingFinished);
	}
}

static void tearDownCoffeePowderDispenser(void *activity) {
	logInfo("[coffeePowderDispenser] Tearing down...");
	destroyActivity(fillStateMonitor);
	destroyActivity(motorController);
}

static void setUpFillStateMonitor(void *activity) {
	logInfo("[fillStateMonitor] Setting up...");
	lastHasBeansState = hasBeans();
}

static void runFillStateMonitor(void *activity) {
	logInfo("[fillStateMonitor] Running...");

	while (TRUE) {
		checkBeans();
		usleep(100);
	}
}

static void tearDownFillStateMonitor(void *activity) {
	logInfo("[coffeePowderDispenser] Tearing down...");
}

static void setUpMotorController(void *activity) {
	logInfo("[motorController] Setting up...");
	setMotor(0);
}

static void runMotorController(void *activity) {
	logInfo("[motorController] Running...");

	while (TRUE) {

		logInfo("[motorController] Going to receive message...");
		MotorControllerMessage message;
		unsigned long messageLength = receiveMessage(activity, (char *)&message, sizeof(message));
		logInfo("[motorController] Message received from %s (length: %ld): value: %d, message: %s",
				message.activity.name, messageLength, message.intValue, message.strValue);
		switch (message.intValue) {
			case MOTOR_START_COMMAND:
				setMotor(50);
				break;
			case MOTOR_STOP_COMMAND:
				setMotor(0);
				break;
		}
	}
}

static void tearDownMotorController(void *activity) {
	logInfo("[motorController] Tearing down...");
	setMotor(0);
}







