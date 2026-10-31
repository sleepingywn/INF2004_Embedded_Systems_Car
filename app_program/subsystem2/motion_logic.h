#ifndef MOTION_LOGIC_H
#define MOTION_LOGIC_H
#include <stdbool.h>
#include <stdint.h>
#include "motion_bits.h"
#include "../subsystem3/ir_types.h"

typedef struct
{
    uint32_t integral_bits : 21; /* signed value encoded explicitly */
    uint16_t error_bits : 13;
    bool primed : 1;
} motion_pid_t;

void motion_pid_reset(motion_pid_t *pid);
int16_t motion_pid_step(motion_pid_t *pid, int16_t target, int16_t actual, uint16_t dt_ms,
                        uint16_t kp, uint16_t ki, uint16_t kd, uint16_t limit);
int32_t motion_count_delta(uint32_t current, uint32_t previous);
int8_t motion_quadrature_delta(uint8_t previous, uint8_t current);
bool motion_line_targets(int8_t error, uint16_t cruise, uint16_t maximum, uint16_t gain_q10,
                         int16_t *left, int16_t *right);
/* Decode exactly *A*, *B*, *C* or *D*, in either scan direction. Zero invalid.
 * No allocation; widths in sample units, no assumption about vehicle speed. */
char motion_barcode_decode(const bc_msg_t *message);
#endif
