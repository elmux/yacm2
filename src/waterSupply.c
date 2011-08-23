/**
 * @brief   Submodule water supply
 * @file    waterSupply.c
 * @version 1.0
 * @authors	Toni Baumann (bauma12@bfh.ch), Ronny Stauffer (staur3@bfh.ch), Elmar Vonlanthen (vonle1@bfh.ch)
 * @date    Aug 15, 2011
 */

/*
 * Zufuhr: Supply (main Thread)
 */

#include <stdio.h>
#include "defines.h"
#include "syslog.h"
#include "sensor.h"
#include "waterSupply.h"

static void setUpWaterSupply(void *activity);
static void runWaterSupply(void *activity);
static void tearDownWaterSupply(void *activity);

static ActivityDescriptor waterSupply = {
	.name = "waterSupply",
	.setUp = setUpWaterSupply,
	.run = runWaterSupply,
	.tearDown = tearDownWaterSupply
};

typedef enum {
	wsState_off = 0,
	wsState_standby,
	wsState_pumpOn,
	wsState_heaterOn,
	wsState_error
} waterSupplyState;

typedef enum {
	devState_off = 0,
	devState_on
} deviceState;

ActivityDescriptor getWaterSupplyDescriptor() {
	return waterSupply;
}

static void setUpWaterSupply(void *activity) {
	printf("Set up Water supply...\n");
}

static int hasWater(void) {
	return readNonBlockableSensor("./dev/waterSensor");
}

static int hasFlow(void) {
	return readNonBlockableSensor("./dev/waterFlowSensor");
}

static int hasTemp(void) {
	int minTemp = 60;
	int curTemp = readNonBlockableSensor("./dev/waterTemperatureSensor");
	return (curTemp >= minTemp) ? TRUE : FALSE;
}

static int controlPump(deviceState state) {
	switch(state) {
	case(devState_on):
		logInfo("Water pump started");
		break;
	case(devState_off):
		logInfo("Water pump stopped");
		break;
	}
	return TRUE;
}

static int controlHeater(deviceState state) {
	switch(state) {
	case(devState_on):
		logInfo("Water heater started");
		break;
	case(devState_off):
		logInfo("Water heater stopped");
		break;
	}
	return TRUE;
}

static void runWaterSupply(void *activity) {
	waterSupplyState state = wsState_off;
	int deliverWater = FALSE;
	WaterSupplyMessage message;
	unsigned long msgLen;

	printf("Running Water supply...\n");
	logInfo("Running Water supply");
	while(TRUE) {
		// read message queue:
		// TODO: should be nonblockable!
		if (!deliverWater) {
			printf("Going to receive message for water supply...\n");
			msgLen = receiveMessage(activity, (char *)&message, sizeof(message));
			if (msgLen > 0) {
				printf("Message length: %ld\n", msgLen);
				printf("Content:\n");
				printf("\tintValue: %d\n", message.intValue);
				printf("\tstringValue: %s\n", message.stringValue);
				if (message.intValue == 42) {
					deliverWater = TRUE;
				}
			}
		}
		// state event machine:
		switch(state) {
		case(wsState_off):
			// TODO: ...
			state = wsState_standby;
			break;
		case(wsState_standby):
			if (deliverWater && hasWater()) {
				if (controlPump(devState_on) < 0) {
					controlPump(devState_off);
					logErr("Could not start water pump!");
				} else {
					state = wsState_pumpOn;
				}
			} else {
				controlPump(devState_off);
			}
			break;
		case(wsState_pumpOn):
			if (deliverWater && hasWater() && hasFlow()) {
				if (controlHeater(devState_on) < 0) {
					controlHeater(devState_off);
					state = wsState_error;
					logErr("Could not start heater!");
				} else {
					state = wsState_heaterOn;
				}
			} else {
				controlPump(devState_off);
				controlHeater(devState_off);
				state = wsState_standby;
			}
			break;
		case(wsState_heaterOn):
			if (!deliverWater || !hasWater()) {
				controlPump(devState_off);
				controlHeater(devState_off);
			}
			if (!hasFlow() || hasTemp()) {
				if (controlHeater(devState_off) < 0) {
					state = wsState_error;
				} else {
					state = wsState_pumpOn;
				}
			}
			break;
		case(wsState_error):
			// TODO: send message to main controller
			break;
		default:
			logErr("Unknown state %d", state);
		}
		sleep(3);
	}
}

static void tearDownWaterSupply(void *activity) {
	printf("Tear down Water supply...\n");
}
