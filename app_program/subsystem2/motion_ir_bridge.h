#ifndef MOTION_IR_BRIDGE_H
#define MOTION_IR_BRIDGE_H
#include "motion.h"
#include "../subsystem3/sensor_line.h"
#include "../subsystem3/barcode_sense.h"

/* Owned by ONE S3 sampling task. Reuses Buddy3's algorithms unchanged.
 * Not an ADC driver. Call with calibrated normalized values at 1 kHz from a
 * hardware-paced task; tk_dly_tsk(1) is not 1 kHz on this 10 ms kernel tick. */
typedef struct
{
    line_tracker_t tracker;
    bc_capture_t capture;
    uint32_t previous_ms;
    uint32_t epoch;
    uint32_t line_ms;
    uint8_t sequence : 4;
    uint8_t primed : 1;
} motion_ir_bridge_t;

void motion_ir_bridge_init(motion_ir_bridge_t *bridge);
/* sample_ms uses motion_now_ms() clock at acquisition. valid must include S3
 * calibration validity. epoch is copied from a coherent motion status record.
 * On ANY gap reset partial barcode; caller must handle a negative return. */
ER motion_ir_bridge_sample(motion_ir_bridge_t *bridge, const uint8_t normalized[3], bool valid,
                           uint32_t sample_ms, uint32_t epoch);
#endif
