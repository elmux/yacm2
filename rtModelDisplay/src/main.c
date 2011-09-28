/**
 * @brief   Main
 * @file    main.c
 * @version 1.0
 * @authors	Toni Baumann (bauma12@bfh.ch), Ronny Stauffer (staur3@bfh.ch), Elmar Vonlanthen (vonle1@bfh.ch)
 * @date    Sep 22, 2011
 */

#include <unistd.h>
#include <signal.h>
#include <sys/mman.h>
#include <defines.h>
#include <activity.h>
#include <device.h>
#include "rtModelDisplay.h"
#include "display.h"

#define COFFEE_GRINDER_MOTOR_DEVICE_FILE "/dev/coffeeGrinderMotor"

static void handleCtrlC(int sig)
{
	// Do nothing here
}

int main(int argc, char **argv) {
	mlockall(MCL_CURRENT|MCL_FUTURE);

	if (argc > 1) {
		setUpDisplay();

		writeNonBlockingDevice(COFFEE_GRINDER_MOTOR_DEVICE_FILE, "50", wrm_replace, FALSE);

		writeDisplay(argv[1]);
		joinDisplay();

		writeNonBlockingDevice(COFFEE_GRINDER_MOTOR_DEVICE_FILE, "0", wrm_replace, FALSE);

		tearDownDisplay();
	} else {
		Activity *rtModelDisplay = createActivity(getRtModelDisplayDescriptor(), messageQueue_blocking);

		// Establish the signal handler
		signal(SIGINT, handleCtrlC);

		// Wait for SIGINT signal
		pause();

		destroyActivity(rtModelDisplay);
	}

	return 0;
}

