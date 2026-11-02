#ifndef S1_TELEMETRY_TIME_H
#define S1_TELEMETRY_TIME_H
#include <stdint.h>
#include <stdbool.h>
/* Terrain's 15-bit timestamp is in the kernel domain. Only the unchanged-
 * snapshot watchdog uses hardware time. Never subtract across domains. */
static inline uint32_t s1_terrain_age(uint32_t kernel_now, uint16_t stamp)
{ return (kernel_now - stamp) & 0x7fffu; }
static inline bool s1_terrain_fresh(uint32_t kernel_now, uint16_t stamp,
                                   uint32_t hardware_now, uint32_t last_change,
                                   uint32_t limit)
{
    return s1_terrain_age(kernel_now,stamp) <= limit &&
        (uint32_t)(hardware_now-last_change) <= limit;
}
#endif
