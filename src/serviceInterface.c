/**
 * @brief   Controlling over service interface
 * @file    serviceInterface.c
 * @version 1.0
 * @authors	Toni Baumann (bauma12@bfh.ch), Ronny Stauffer (staur3@bfh.ch), Elmar Vonlanthen (vonle1@bfh.ch)
 * @date    Aug 15, 2011
 */

#include <stdio.h>
#include "defines.h"
#include "syslog.h"
#include "device.h"
#include "mainController.h"
#include "serviceInterface.h"

static void setUpServiceInterface(void *activity);
static void runServiceInterface(void *activity);
static void tearDownServiceInterface(void *activity);

static ActivityDescriptor serviceInterface = {
	.name = "serviceInterface",
	.setUp = setUpServiceInterface,
	.run = runServiceInterface,
	.tearDown = tearDownServiceInterface
};

ActivityDescriptor getServiceInterfaceDescriptor() {
	return serviceInterface;
}

static void setUpServiceInterface(void *activity) {
	printf("[serviceInterface] Setting up...\n");
}

static void runServiceInterface(void *activity) {
	printf("[serviceInterface] Running...\n");

	while (TRUE) {
		printf("[serviceInterface] Going to receive message...\n");
		ServiceInterfaceMessage message;
		unsigned long messageLength = receiveMessage(activity, (char *)&message, sizeof(message));
		printf("[serviceInterface] Message received - length: %ld, value: %d, message: %s\n",
				messageLength, message.intValue, message.strValue);
	}
}

static void tearDownServiceInterface(void *activity) {
	printf("[serviceInterface] Tearing down...\n");
}

