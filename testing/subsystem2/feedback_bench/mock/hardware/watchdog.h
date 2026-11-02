#include <stdint.h>
#include <stdbool.h>
void watchdog_enable(uint32_t delay_ms,bool pause_on_debug);
void watchdog_update(void);
void watchdog_disable(void);
