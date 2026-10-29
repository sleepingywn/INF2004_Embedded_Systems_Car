#ifndef DRV_ENCODER_H
#define DRV_ENCODER_H
#include "../app.h"
#include <stdint.h>
typedef struct
{
    uint32_t left;
    uint32_t right;
    uint32_t invalid;
} encoder_raw_t;
/* Init and snapshot ONLY on the motion task's core. IRQ is enabled on that
 * core alone; other tasks consume motion_get_status(), never these counters. */
ER encoder_hw_init(void);
void encoder_hw_snapshot(encoder_raw_t *snapshot);
#endif
