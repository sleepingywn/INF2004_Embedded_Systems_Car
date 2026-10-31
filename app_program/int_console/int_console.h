/*
 *	int_console.h
 *	Integration test console (development tool, not a subsystem).
 *
 *	A low-priority task that prints the live state of subsystems 2-4 and
 *	takes single-key commands from the USB serial monitor, so the motion
 *	(B2) and IR (B3) code can be exercised on the car before the Vehicle
 *	Controller exists.  Press 'h' in the serial monitor for the key list.
 *
 *	Set APP_CONSOLE to 0 to leave it out of the firmware.
 */

#ifndef INT_CONSOLE_H
#define INT_CONSOLE_H

#include "../app.h"

#ifndef APP_CONSOLE
#define APP_CONSOLE		1
#endif

#define CONSOLE_PRI		22	/* below every subsystem band (12-21) */
#define CONSOLE_PERIOD_MS	100	/* key polling                        */
#define CONSOLE_PRINT_MS	500	/* status line                        */

IMPORT ER int_console_start(void);

#endif /* INT_CONSOLE_H */
