#ifndef APP_TASK_LED_H
#define APP_TASK_LED_H

#include <tk/tkernel.h>

IMPORT void led_task(INT stacd, void *exinf);
IMPORT void led_cyclic_handler(void *exinf);

#endif
