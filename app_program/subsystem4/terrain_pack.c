/*
 *	terrain_pack.c
 *	Subsystem 4 -- pack terrain values into the 64-bit S4_TERRAIN.
 *	See terrain_pack.h for the field table.
 */

#include "terrain_pack.h"

/* Round to the nearest integer, then clamp to [lo, hi].  No libm needed. */
static int32_t round_clamp(float x, int32_t lo, int32_t hi)
{
	int32_t r;

	if (x >  1.0e6f) x =  1.0e6f;		/* keep the cast in range */
	if (x < -1.0e6f) x = -1.0e6f;
	r = (int32_t)(x >= 0.0f ? x + 0.5f : x - 0.5f);
	if (r < lo) r = lo;
	if (r > hi) r = hi;
	return r;
}

S4_TERRAIN terrain_pack(const terrain_values_t *v)
{
	S4_TERRAIN t;

	t.pitch_hd     = round_clamp(v->pitch_deg * 2.0f, -128, 127);	/* 0.5 deg */
	t.roll_hd      = round_clamp(v->roll_deg  * 2.0f, -128, 127);
	t.turn_2dps    = v->turn_valid
		       ? round_clamp(v->turn_dps * 0.5f, -127, 127)	/* 2 deg/s */
		       : S4_TURN_NA;					/* -128 reserved */
	t.cur_peak_mm  = (uint64_t)round_clamp(v->cur_peak_mm,  0, 127);
	t.high_peak_mm = (uint64_t)round_clamp(v->high_peak_mm, 0, 127);
	t.hump_state   = (v->hump_state <= 2) ? v->hump_state : 0;
	t.motion_class = v->motion_class & 0x7u;
	t.hump_count   = v->hump_count & 0xFu;			/* wraps at 16 */
	t.calibrated   = v->calibrated ? 1u : 0u;
	t.imu_fault    = v->imu_fault ? 1u : 0u;
	t.time_ms      = v->time_ms & S4_TIME_MASK;			/* wraps at 32768 */
	return t;
}
