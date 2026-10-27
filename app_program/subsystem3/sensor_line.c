/*
 *	sensor_line.c
 *	Subsystem 3 -- IR calibration and double-line proportional tracing.
 *	See sensor_line.h.  No kernel or hardware calls.
 */

#include "sensor_line.h"

#define ADC_MAX		4095u

/* ------------------------------------------------------------------ */
void ir_cal_begin(ir_cal_acc_t *acc)
{
	for (uint8_t ch = 0; ch < IR_NUM_CH; ch++) {
		acc->min[ch] = ADC_MAX;
		acc->max[ch] = 0;
	}
}

void ir_cal_add(ir_cal_acc_t *acc, const uint16_t raw[IR_NUM_CH])
{
	for (uint8_t ch = 0; ch < IR_NUM_CH; ch++) {
		uint16_t r = (raw[ch] > ADC_MAX) ? ADC_MAX : raw[ch];
		if (r < acc->min[ch]) acc->min[ch] = r;
		if (r > acc->max[ch]) acc->max[ch] = r;
	}
}

uint8_t ir_cal_finish(const ir_cal_acc_t *acc, ir_cal_t cal[IR_NUM_CH])
{
	uint8_t failed = 0;

	for (uint8_t ch = 0; ch < IR_NUM_CH; ch++) {
		uint16_t lo = acc->min[ch];
		uint16_t hi = acc->max[ch];

		if (hi < lo) {			/* no samples at all */
			lo = hi = 0;
		}
#if IR_BLACK_IS_HIGH
		cal[ch].white = lo & 0x0FFFu;	/* lo, hi <= 4095 */
		cal[ch].black = hi & 0x0FFFu;
#else
		cal[ch].white = hi & 0x0FFFu;
		cal[ch].black = lo & 0x0FFFu;
#endif
		if ((uint16_t)(hi - lo) < IR_CAL_MIN_CONTRAST) {
			failed |= (uint8_t)(1u << ch);
			/* Not enough contrast: mark uncalibrated (white == black) so
			   ir_normalise() reports 0 and noise can never look like a
			   bar or a line.  The caller raises the IR fault. */
			cal[ch].black = cal[ch].white;
		}
	}
	return failed;
}

uint8_t ir_normalise(const ir_cal_t *cal, uint16_t raw)
{
	int32_t w = (int32_t)cal->white;
	int32_t b = (int32_t)cal->black;

	if (b == w) {
		return 0;			/* uncalibrated: report white */
	}
	int32_t n = ((int32_t)raw - w) * 100 / (b - w);
	if (n < 0)   return 0;
	if (n > 100) return 100;
	return (uint8_t)n;
}

/* ------------------------------------------------------------------ */
void line_init(line_tracker_t *t)
{
	t->last_err = 0;
	t->white_ms = 0;
}

int8_t line_update(line_tracker_t *t, uint8_t n_a, uint8_t n_b,
		   uint16_t dt_ms, line_state_t *state)
{
	bool a_blk = n_a >= LINE_ON_BLACK;
	bool b_blk = n_b >= LINE_ON_BLACK;
	bool a_wht = n_a <= LINE_OFF_WHITE;
	bool b_wht = n_b <= LINE_OFF_WHITE;

	if (a_blk && b_blk) {			/* junction candidate */
		t->white_ms = 0;
		*state = LINE_BOTH_BLACK;
		return 0;
	}

	if (a_wht && b_wht) {			/* line has left both sensors */
		uint32_t w = (uint32_t)t->white_ms + dt_ms;
		t->white_ms = (w > 0xFFFFu) ? 0xFFFFu : (uint16_t)w;
		*state = (t->white_ms >= LINE_LOST_TIMEOUT_MS) ? LINE_LOST : LINE_LOST_HOLD;
		/* Two sensors cannot see where the line went; the last error
		   says which side it left on. */
		return (t->last_err >= 0) ? 100 : -100;
	}

	t->white_ms = 0;
	/* n_a + n_b > 0 here: both being 0 is caught by the both-white case. */
	int16_t e = (int16_t)(100 * ((int16_t)n_a - (int16_t)n_b) /
			      ((int16_t)n_a + (int16_t)n_b));
	*state = ((a_blk && b_wht) || (b_blk && a_wht)) ? LINE_EDGE_ONLY : LINE_ON;
	t->last_err = (int8_t)e;
	return (int8_t)e;
}
