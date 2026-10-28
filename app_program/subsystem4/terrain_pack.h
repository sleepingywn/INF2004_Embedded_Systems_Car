/*
 *	terrain_pack.h
 *	Subsystem 4 -- minimum-bit terrain snapshot (IF-06).
 *
 *	Change requirement "Minimum-Bit Data Representation": every field uses
 *	only the bits its valid range needs (C bit-fields).  Calculations stay
 *	in float inside terrain_task; values are packed only when shared.
 *
 *	Field table (64 bits = 8 bytes; was 304 bits / 38 bytes)
 *	  Field          Valid range                Resolution   Values  Bits
 *	  pitch_hd       -64.0 .. +63.5 deg         0.5 deg       256     8   (signed; saturates)
 *	  roll_hd        -64.0 .. +63.5 deg         0.5 deg       256     8   (signed; saturates)
 *	  turn_2dps      -254 .. +254 deg/s         2 deg/s       255     8   (signed; -128 = not available)
 *	  cur_peak_mm    0 .. 127 mm                1 mm          128     7   (saturates)
 *	  high_peak_mm   0 .. 127 mm                1 mm          128     7   (saturates)
 *	  hump_state     flat / climbing / descending             3       2
 *	  motion_class   up to 8 classes (0 = unknown)            8       3   (reserved, B4-FR11)
 *	  hump_count     0 .. 15, wraps             1             16      4
 *	  calibrated     no / yes                                 2       1
 *	  imu_fault      no / yes                                 2       1
 *	  time_ms        0 .. 32767 ms, wraps       1 ms          32768   15  (age = now - time_ms, 15-bit)
 *	                                                                 ----
 *	                                                                  64
 *	Bits = ceil(log2(number of values)).  Ranges: tilt fires at 25 deg, so
 *	+-63.5 deg is enough; the hump target is +-10 mm, so 1 mm steps and a
 *	127 mm ceiling are enough; staleness is judged at 100 ms, so a 32.7 s
 *	wrap is enough.
 *
 *	Safe access: the 8 bytes are written by terrain_task only and read by
 *	other tasks only through s4_get_terrain(), both under mtx_terrain.  The
 *	RP2040 cannot store 64 bits in one instruction, so the mutex is what
 *	stops a reader seeing half an update.  Nobody does read-modify-write on
 *	single fields of the shared copy: the whole snapshot is built outside
 *	the lock and copied in one assignment.
 *
 *	Plain C (no kernel calls), so the laptop test can check it too:
 *	testing/subsystem4/imu_sim/pack_check.c
 */

#ifndef TERRAIN_PACK_H
#define TERRAIN_PACK_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
	int64_t		pitch_hd	: 8;
	int64_t		roll_hd		: 8;
	int64_t		turn_2dps	: 8;
	uint64_t	cur_peak_mm	: 7;
	uint64_t	high_peak_mm	: 7;
	uint64_t	hump_state	: 2;
	uint64_t	motion_class	: 3;
	uint64_t	hump_count	: 4;
	uint64_t	calibrated	: 1;
	uint64_t	imu_fault	: 1;
	uint64_t	time_ms		: 15;
} S4_TERRAIN;

_Static_assert(sizeof(S4_TERRAIN) == 8, "S4_TERRAIN must pack into 8 bytes");

#define S4_TURN_NA		(-128)		/* turn_2dps: not available     */
#define S4_TIME_MASK		0x7FFFu		/* time_ms is 15 bits           */

/* Unpacked values, as terrain_task computes them */
typedef struct {
	float		pitch_deg, roll_deg;
	float		turn_dps;
	bool		turn_valid;
	float		cur_peak_mm, high_peak_mm;
	uint8_t		hump_state;	/* 0 flat, 1 climbing, 2 descending */
	uint8_t		motion_class;
	uint32_t	hump_count;
	bool		calibrated, imu_fault;
	uint32_t	time_ms;	/* kernel time, ms */
} terrain_values_t;

/* Round and saturate every field into its bit width. */
S4_TERRAIN	terrain_pack(const terrain_values_t *v);

/* ---- decoding, for consumers (integers: tm_printf has no %f) ---- */
#define S4_PITCH_DDEG(t)	((int)(t).pitch_hd * 5)		/* 0.1 deg   */
#define S4_ROLL_DDEG(t)		((int)(t).roll_hd * 5)		/* 0.1 deg   */
#define S4_TURN_DPS(t)		((int)(t).turn_2dps * 2)	/* deg/s; check S4_TURN_OK */
#define S4_TURN_OK(t)		((t).turn_2dps != S4_TURN_NA)
#define S4_VALID(t)		((t).calibrated && !(t).imu_fault)
/* Age of a snapshot in ms, correct across the 15-bit wrap */
#define S4_AGE_MS(t, now_ms)	((unsigned)(((now_ms) - (t).time_ms) & S4_TIME_MASK))

#endif /* TERRAIN_PACK_H */
