/**
 * @brief   Main system control module
 * @file    mainController.c
 * @version 1.0
 * @authors	Toni Baumann (bauma12@bfh.ch), Ronny Stauffer (staur3@bfh.ch), Elmar Vonlanthen (vonle1@bfh.ch)
 * @date    Aug 15, 2011
 */

#include <stdio.h>
#include <unistd.h>
#include "defines.h"
#include "sensor.h"
#include "syslog.h"
#include "coffeeSupply.h"
#include "waterSupply.h"
#include "milkSupply.h"
#include "mainController.h"

static void setUpMainController(void *activity);
static void runMainController(void *activity);
static void tearDownMainController(void *activity);

static const char *subSystemStr[] = {
		"Coffee supply",
		"Milk Supply",
		"Water Supply",
		"Service Interface",
		"User Interface"
};

static ActivityDescriptor mainController = {
		.name = "mainController",
		.setUp = setUpMainController,
		.run = runMainController,
		.tearDown = tearDownMainController
};

ActivityDescriptor getMainControllerDescriptor() {
	return mainController;
}

static void setUpMainController(void *activity) {
	printf("[mainController] Setting up...\n");
}

static void runMainController(void *activity) {
	MainControllerMessage message;
	unsigned long msgLen;
	printf("[mainController] Running...\n");
	sleep(10);

	printf("[mainController] Send message to coffee supply...\n");
	printf("[mainController] Message size: %ld\n", sizeof(CoffeeSupplyMessage));
	sendMessage(getCoffeeSupplyDescriptor(), (char *)&(CoffeeSupplyMessage){
		.intValue = 123,
		.strValue = "abc"
		}, sizeof(CoffeeSupplyMessage), prio_medium);
	printf("[mainController] ...done. (send message)\n");

	printf("[mainController] Send message to water supply...\n");
	sendMessage(getWaterSupplyDescriptor(), (char *)&(WaterSupplyMessage) {
		.intValue = 1,
		.strValue = "Start water supply",
	}, sizeof(WaterSupplyMessage), prio_medium);

	while(TRUE) {
		printf("[mainController] Waiting for subsystem messages...\n");
		msgLen = receiveMessage(activity, (char *)&message, sizeof(message));
		if (msgLen > 0) {
			printf("[mainController] Message from %s (%ld): intVal=%d, strVal='%s'\n",
					subSystemStr[message.subSystem],
					msgLen,
					message.intValue,
					message.strValue);
		}
	}
}

static void tearDownMainController(void *activity) {
	printf("[mainController] Tearing down...\n");
}
