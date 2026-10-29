#ifndef IR_ACQUISITION_H
#define IR_ACQUISITION_H

#include "../app.h"
#include "ir_types.h"
#include <stdbool.h>

/* Private to the S3 task on core 0 and its two same-priority ISRs. */
#define IR_ACQ_EVENT 1u
#define IR_ACQ_RING_SIZE 16u /* 15 usable slots; overflow stops acquisition */
#define IR_ACQ_PERIOD_US 1000u
/* Provisional acceptance bound, not a measured timing achievement. */
#define IR_ACQ_MAX_JITTER_US 100u

typedef enum
{
    IR_ACQ_HARDWARE = 1u,
    IR_ACQ_TIMING = 2u,
    IR_ACQ_ADC = 4u,
    IR_ACQ_OVERFLOW = 8u
} ir_acq_fault_t;

typedef struct
{
    uint32_t sampled_ms; /* scheduled acquisition time, motion_now_ms timebase */
    uint32_t barcode : 12;
    uint32_t line_a : 12;
    uint32_t line_b : 12;
} ir_raw_frame_t;

typedef struct
{
    uint32_t samples;
    uint32_t max_lateness_us;
    uint32_t max_scan_us;
    uint8_t faults : 4;
    bool running : 1;
} ir_acq_status_t;

/* Called ONLY by the pinned S3 task. No other subsystem reads ADC registers. */
ER ir_acquisition_start(ID event);
void ir_acquisition_stop(void);
bool ir_acquisition_pop(ir_raw_frame_t *frame, ir_acq_status_t *status);
void ir_acquisition_discard(void);

#endif
