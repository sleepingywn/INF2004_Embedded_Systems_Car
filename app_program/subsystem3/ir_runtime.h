#ifndef IR_RUNTIME_H
#define IR_RUNTIME_H

#include "ir_acquisition.h"

typedef enum
{
    IR_RUNTIME_UNCALIBRATED,
    IR_RUNTIME_CALIBRATING,
    IR_RUNTIME_READY,
    IR_RUNTIME_FAULT
} ir_runtime_state_t;

typedef enum
{
    IR_RUNTIME_BEGIN = 1u,
    IR_RUNTIME_FINISH = 2u
} ir_runtime_command_t;

typedef struct
{
    uint32_t updated_ms;
    uint32_t epoch;
    uint32_t calibration_samples;
    uint32_t handoff_errors;
    ir_raw_frame_t raw;
    ir_cal_t calibration[IR_NUM_CH];
    ir_acq_status_t acquisition;
    struct
    {
        uint8_t value : 7; /* 0..100, meaningful only in READY */
    } normalized[IR_NUM_CH];
    uint8_t state : 2;
    uint8_t failed_channels : 3; /* bit 0 GP26, bit 1 GP27, bit 2 GP28 */
    bool healthy : 1;
    bool command_pending : 1;
} ir_runtime_status_t;

ER ir_runtime_init(void);
ER ir_runtime_start(void);
/* Task-context API, nonblocking copied command. E_OK means accepted; inspect
 * status to observe completion. BEGIN stops motion, clears old calibration,
 * and restarts acquisition after a fault. FINISH commits measured min/max.
 * Neither command resets the latched B2 fault or starts the motors. */
ER ir_runtime_calibration(ir_runtime_command_t command);
ER ir_runtime_get_status(ir_runtime_status_t *status);

#endif
