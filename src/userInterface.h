/**
 * @brief   User interface
 * @file    userInterface.h
 * @version 1.0
 * @authors	Toni Baumann (bauma12@bfh.ch), Ronny Stauffer (staur3@bfh.ch), Elmar Vonlanthen (vonle1@bfh.ch)
 * @date    Aug 22, 2011
 */

#ifndef USERINTERFACE_H_
#define USERINTERFACE_H_

#include <mqueue.h>
#include "activity.h"

typedef struct {
	int intValue;
	char strValue[256];
} UserInterfaceMessage;

extern ActivityDescriptor getUserInterfaceDescriptor(void);

#endif /* USERINTERFACE_H_ */
