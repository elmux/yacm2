/**
 * @brief   User interface
 * @file    userInterface.c
 * @version 1.0
 * @authors	Toni Baumann (bauma12@bfh.ch), Ronny Stauffer (staur3@bfh.ch), Elmar Vonlanthen (vonle1@bfh.ch)
 * @date    Aug 15, 2011
 */

/*
 * Benutzerfuehrung: Controller (Main Thread)
 * Ausgabe: Output
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/epoll.h>
#include "defines.h"
#include "data.h"
#include "syslog.h"
#include "device.h"
#include "display.h"
#include "activity.h"
#include "mainController.h"
#include "userInterface.h"

/**
 * Define switches and buttons
 */
#define POWER_SWITCH 1<<0
#define MILK_SELECTOR_SWITCH 1<<1
#define PRODUCT_3_BUTTON 2
#define PRODUCT_2_BUTTON 1
#define PRODUCT_1_BUTTON 0

static void setUpUserInterface(void *activity);
static void runUserInterface(void *activity);
static void tearDownUserInterface(void *activity);

static ActivityDescriptor userInterfaceDescriptor = {
	.name = "userInterface",
	.setUp = setUpUserInterface,
	.run = runUserInterface,
	.tearDown = tearDownUserInterface
};

MESSAGE_CONTENT_TYPE_MAPPING(UserInterface, Command, 1)
MESSAGE_CONTENT_TYPE_MAPPING(UserInterface, ChangeViewCommand, 2)
MESSAGE_CONTENT_TYPE_MAPPING(UserInterface, Status, 3)

static Activity *this;
static Activity *display;

static unsigned int powerState = FALSE;
static MachineState machineState = machineState_off;
static unsigned int withMilk = FALSE;
static Availability coffeeAvailability = available;
static Availability waterAvailability = available;
static Availability milkAvailability = available;
static unsigned int productIndex = 0;
static unsigned int wasteBinFull = FALSE;

static int switchesStates = 0;
static int switchesPreviousStates = 0;

ActivityDescriptor getUserInterfaceDescriptor() {
	return userInterfaceDescriptor;
}

static void setUpUserInterface(void *activity) {
	logInfo("[UserInterface] Setting up...");
	display = createActivity(getDisplayDescriptor(), messageQueue_blocking);
	this = (Activity *)activity;
}

static void notifyDisplay() {
	logInfo("[%s] Notifiying display...", this->descriptor->name);
	sendRequest_BEGIN(this, Display, ChangeViewCommand)
		.powerState = powerState,
		.machineState = machineState,
		.withMilk = withMilk,
		.coffeeAvailability = coffeeAvailability,
		.waterAvailability = waterAvailability,
		.milkAvailability = milkAvailability,
		.wasteBinFull = wasteBinFull,
		.productIndex = productIndex
	sendRequest_END
}

static void processEventSwitchesChanges(int switchesStates) {
	int switchesChanges;

	// Get bitfield of changed switches with XOR:
	switchesChanges = (switchesStates^switchesPreviousStates);

	// Check if the status of the power switch has changed:
	if (switchesChanges & POWER_SWITCH) {
		if (switchesStates & POWER_SWITCH) {
			// send init command to mainController:
			sendRequest_BEGIN(this, MainController, InitCommand)
			sendRequest_END
			powerState = TRUE;
		} else {
			// send off command to mainController:
			sendRequest_BEGIN(this, MainController, OffCommand)
			sendRequest_END
			powerState = FALSE;
		}
	}
	// Check if the status of the milk selector switch has changed:
	if (switchesChanges & MILK_SELECTOR_SWITCH) {
		if (switchesStates & MILK_SELECTOR_SWITCH) {
			withMilk = TRUE;
		} else {
			withMilk = FALSE;
		}
	}
	switchesPreviousStates = switchesStates;
}

static void runUserInterface(void *activity) {
	int polling, fd, i, result;
	struct epoll_event firedEvents[100];
	int numberOfFiredEvents;
	char buffer[3];
	char buttonsEventDevice[] = "/dev/buttonsEvent";
	int buttonsFileDescriptor;
	char switchesDevice[] = "/dev/switches";
	char switchesEventDevice[] = "/dev/switchesEvent";
	int switchesFileDescriptor;
	//MainControllerMessage mainControllerMessage;
	//mainControllerMessage.activity = getUserInterfaceDescriptor();

	logInfo("[userInterface] Running...");

	// initially read switches states and process event:
	logInfo("[userInterface] Checking initial switch states...");
	switchesStates = readNonBlockingDevice(switchesDevice);
	processEventSwitchesChanges(switchesStates);
	if (switchesStates) {
		notifyDisplay();
	}

	// open file descriptors for buttons and switches:
	logInfo("[userInterface] Open file descriptors...");
	buttonsFileDescriptor = open(buttonsEventDevice, O_RDONLY);
	if (buttonsFileDescriptor < 0) {
		logErr("[%s] Unable to open device %s: %s", (*this->descriptor).name, buttonsEventDevice, strerror(errno));
		return;
	}
	switchesFileDescriptor = open(switchesEventDevice, O_RDONLY);
	if (switchesFileDescriptor < 0) {
		logErr("[%s] Unable to open device %s: %s", (*this->descriptor).name, switchesEventDevice, strerror(errno));
		return;
	}

	// create poll device:
	logInfo("[userInterface] Creating polling device...");
	if ((polling = epoll_create(3)) < 0) {
		logErr("[%s] Error setting up event waiting: %s", this->descriptor->name, strerror(errno));
	}
	struct epoll_event eventDescriptors[] = {
		{ .events = EPOLLIN, .data.fd = this->messageQueue },
		{ .events = EPOLLIN, .data.fd = buttonsFileDescriptor },
		{ .events = EPOLLIN, .data.fd = switchesFileDescriptor }
	};
	for (i = 0; i < 3; i++) {
		if (epoll_ctl(polling, EPOLL_CTL_ADD, eventDescriptors[i].data.fd, &eventDescriptors[i]) < 0) {
			logErr("[%s] Error registering event source %d (fd %d): %s", this->descriptor->name, i, eventDescriptors[i].data.fd, strerror(errno));
			close(polling);
			close(buttonsFileDescriptor);
			close(switchesFileDescriptor);
			return;
		}
	}

	while (TRUE) {
		// wait for events:
		numberOfFiredEvents = epoll_wait(polling, firedEvents, 100, -1);
		if (numberOfFiredEvents < 0) {
			logErr("[%s] Error waiting for event: %s", this->descriptor->name, strerror(errno));
		} else {
			logInfo("[%s] Events occured", this->descriptor->name);
			for (i = 0; i < numberOfFiredEvents; i++) {
				fd = firedEvents[i].data.fd;
				if (fd == this->messageQueue) {
					receiveGenericMessage_BEGIN(this)
						if (error) {
							//TODO Implement appropriate error handling
							sleep(10);

							// Try again
							continue;
						}
						logInfo("[%s] Message received from %s (message length: %u)", this->descriptor->name, senderDescriptor.name, result);
						MESSAGE_SELECTOR_BEGIN
							MESSAGE_BY_SENDER_SELECTOR(senderDescriptor, message, Display)
								MESSAGE_SELECTOR_BEGIN
									MESSAGE_BY_TYPE_SELECTOR(*specificMessage, Display, Result)
										logInfo("[%s] Display result received!", this->descriptor->name);
								MESSAGE_SELECTOR_END
							MESSAGE_BY_SENDER_SELECTOR(senderDescriptor, message, MainController)
								MESSAGE_SELECTOR_BEGIN
									MESSAGE_BY_TYPE_SELECTOR(*specificMessage, MainController, MachineStateChangedNotification)
										// Process received machine state and update display
										machineState = content.state;
									MESSAGE_BY_TYPE_SELECTOR(*specificMessage, MainController, IngredientAvailabilityChangedNotification)
										// Process received ingredient availability and update display
										switch (content.ingredientIndex) {
										case COFFEE_INDEX:
											coffeeAvailability = content.availability;
											break;
										case WATER_INDEX:
											waterAvailability = content.availability;
											break;
										case MILK_INDEX:
											milkAvailability = content.availability;
										}
										notifyDisplay();
									MESSAGE_BY_TYPE_SELECTOR(*specificMessage, MainController, CoffeeWasteBinStateChangedNotification)
										wasteBinFull = content.isBinFull;
										// TODO: LED einschalten und keine neuen Auftrage akzeptieren
								MESSAGE_SELECTOR_END
						MESSAGE_SELECTOR_END
					receiveGenericMessage_END
				} else if(fd == buttonsFileDescriptor) {
					result = read(buttonsFileDescriptor, buffer, 3);
					if (result < 0) {
						logErr("[%s] read button: %s", this->descriptor->name, strerror(errno));
					}
					productIndex = (atoi(buffer))+1;
					if (productIndex > 0 && productIndex <= 3) {
						// TODO: Update display and send command
						sendRequest_BEGIN(this, MainController, ProduceProductCommand)
							.productIndex = productIndex,
							.withMilk = withMilk
						sendRequest_END
						notifyDisplay();
					} else {
						logWarn("[%s] No product at index %d", this->descriptor->name, productIndex);
					}
				} else if(fd == switchesFileDescriptor) {
					// Get bitfield of current switches status:
					result = read(switchesFileDescriptor, buffer, 3);
					if (result < 0) {
						logErr("[%s] Read button: %s", this->descriptor->name, strerror(errno));
					}
					switchesStates = atoi(buffer);
					processEventSwitchesChanges(switchesStates);
					notifyDisplay();
				}
			} // foreach event end
		} // if number of fired events end
	}
	close(polling);
	close(buttonsFileDescriptor);
	close(switchesFileDescriptor);
}

static void tearDownUserInterface(void *activity) {
	logInfo("[userInterface] Tearing down...");
	destroyActivity(display);
}
