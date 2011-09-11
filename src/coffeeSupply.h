/**
 * @brief   Submodule coffee supply
 * @file    coffeeSupply.h
 * @version 1.0
 * @authors	Toni Baumann (bauma12@bfh.ch), Ronny Stauffer (staur3@bfh.ch), Elmar Vonlanthen (vonle1@bfh.ch)
 * @date    Aug 15, 2011
 */

#ifndef COFFEESUPPLY_H_
#define COFFEESUPPLY_H_

#include <mqueue.h>
#include "activity.h"

typedef struct {
	ActivityDescriptor activity;
	int intValue;
	char strValue[256];
} CoffeeSupplyMessage;

typedef struct {
	ActivityDescriptor activity;
	int intValue;
	char strValue[256];
} CoffeePowderDispenserMessage;

typedef struct {
	ActivityDescriptor activity;
	int intValue;
	char strValue[256];
} FillStateMonitorMessage;

typedef struct {
	ActivityDescriptor activity;
	int intValue;
	char strValue[256];
} MotorControllerMessage;


extern ActivityDescriptor getCoffeeSupplyDescriptor(void);

/**
 * Represents a coffeeSupply event
 */
typedef enum {
	coffeeSupplyEvent_init,
	coffeeSupplyEvent_switchOff,
	coffeeSupplyEvent_initialized,
	coffeeSupplyEvent_startSupplying,
	coffeeSupplyEvent_supplyingFinished,
	coffeeSupplyEvent_stop,
} CoffeeSupplyEvent;


#endif /* COFFEESUPPLY_H_ */
