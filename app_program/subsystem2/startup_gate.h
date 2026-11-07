#ifndef MOTION_STARTUP_GATE_H
#define MOTION_STARTUP_GATE_H
#include <stdint.h>
#include <stdbool.h>
#define MOTION_STARTUP_LIMIT_MS 5000u
/* Results are independent: late release must still latch a timeout fault. */
typedef struct { bool release; bool timeout; } motion_startup_decision_t;
static inline motion_startup_decision_t motion_startup_decide(uint32_t start,
                                                               uint32_t now,
                                                               bool app_ready, bool radio_initialized)
{
    motion_startup_decision_t result = {app_ready && radio_initialized,
        (uint32_t)(now - start) > MOTION_STARTUP_LIMIT_MS};
    return result;
}
#endif
