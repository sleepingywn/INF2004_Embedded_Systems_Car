/*
 *	sensor_line.h
 *	Subsystem 3 -- IR calibration and double-line proportional tracing.
 *
 *	Hardware- and kernel-independent: the caller reads the ADC and passes
 *	raw values in.  Used by the S3 task and by the bench test.
 */

#ifndef SENSOR_LINE_H
#define SENSOR_LINE_H

#include <stdbool.h>
#include <stdint.h>
#include "ir_types.h"

/* ---- Calibration (B3-FR2) ------------------------------------------
 *  1. ir_cal_begin(), then ir_cal_add() for every raw sample while each
 *     sensor is moved over white AND black.
 *  2. ir_cal_finish() fills cal[] and returns a bitmask of channels that
 *     did not see enough contrast (bit ch set = failed).  0 = all good.
 *     A failed channel is left uncalibrated (white == black), so
 *     ir_normalise() returns 0 for it: the caller must raise IR_FAULT.
 * --------------------------------------------------------------------- */
typedef struct {
	uint16_t	min[IR_NUM_CH];
	uint16_t	max[IR_NUM_CH];
} ir_cal_acc_t;

void	ir_cal_begin(ir_cal_acc_t *acc);
void	ir_cal_add(ir_cal_acc_t *acc, const uint16_t raw[IR_NUM_CH]);
uint8_t	ir_cal_finish(const ir_cal_acc_t *acc, ir_cal_t cal[IR_NUM_CH]);

/* Raw 12-bit reading -> 0 (white) .. 100 (black).  Works for either
   sensor polarity.  Safe to call from an ISR (no state). */
uint8_t	ir_normalise(const ir_cal_t *cal, uint16_t raw);

/* ---- Line error (B3-FR3, B3-FR5) ------------------------------------
 *  Call every period with the normalised readings of line sensor A
 *  (left) and B (right).  Result -100 .. +100, + = line to the LEFT.
 *
 *    LINE_MODE_OUTSIDE:  e = nA - nB.  Both white = centred (e = 0,
 *                        LINE_ON); LINE_LOST after LINE_UNSEEN_MS with
 *                        neither sensor seeing the line.
 *    LINE_MODE_EDGE:     e = 100 * (nA - nB) / (nA + nB).  Dividing by
 *                        the sum cancels changes common to both sensors
 *                        (e.g. height on a hump).  Both white = line lost.
 * --------------------------------------------------------------------- */
typedef struct {
	int8_t		last_err;	/* EDGE: last error while the line was seen */
	uint16_t	white_ms;	/* how long the line has not been seen      */
} line_tracker_t;

void	line_init(line_tracker_t *t);
int8_t	line_update(line_tracker_t *t, uint8_t n_a, uint8_t n_b,
		    uint16_t dt_ms, line_state_t *state);

#endif /* SENSOR_LINE_H */
