/*
 *	subsystem4.h
 *	Subsystem 4 -- IMU / terrain monitoring.  Owner: Andy (Buddy 4).
 *
 *	Public interface of subsystem 4.  Other subsystems include it as
 *	"../subsystem4/subsystem4.h".
 *
 *	What subsystem 4 does
 *	  terrain_task (priority 18) is woken every 20 ms by a cyclic handler,
 *	  reads the GY-511 accelerometer over I2C0 (GP16 SDA / GP17 SCL) and
 *	  runs imu_logic: pitch, roll, hump detection and height, tilt and
 *	  collision.  The CAR MUST STAND STILL ON LEVEL GROUND FOR THE FIRST
 *	  2 s after power-up: that is the calibration.
 *
 *	What other subsystems can use
 *	  IF-06  s4_get_terrain()  latest terrain snapshot, copied under a mutex
 *	  IF-07  flg_terrain       event flag: hump, tilt, collision, IMU fault
 */

#ifndef SUBSYSTEM4_H
#define SUBSYSTEM4_H

#include "../app.h"

#define S4_PRI_TERRAIN		18	/* priority band 18-19 (TEAM.md section 4) */
#define S4_PERIOD_MS		20	/* terrain task period                     */
/* Core for terrain_task.  Pinned to core 1, the core that handles the
   I2C interrupt, so the driver's "transfer done" wake-up is immediate.
   Measured 2026-10-31 on the car: on core 0 every read took ~10-11 ms
   (it waited for the next 10 ms kernel tick, overruns); on core 1 the
   worst read was 0.76 ms, 0 slow cycles, 0 overruns. */
#ifndef S4_CORE
#define S4_CORE			TP_PRC2		/* core 1 */
#endif

/* ---------------------------------------------------------------------
 *  IF-06: terrain snapshot (owned by S4)
 *  S4_TERRAIN is the minimum-bit, 64-bit (8-byte) packed snapshot; its
 *  field table and decoding macros (S4_PITCH_DDEG, S4_VALID, S4_AGE_MS...)
 *  are in terrain_pack.h.
 * --------------------------------------------------------------------- */
#include "terrain_pack.h"

/* Copy the latest snapshot.  Waits at most 2 ms for the mutex.
   Returns E_OK, or an error (then *out is unchanged).  Task context only.
   Example:
	S4_TERRAIN t;  SYSTIM now;
	if (s4_get_terrain(&t) == E_OK && S4_VALID(t)) {
		tk_get_tim(&now);
		if (S4_AGE_MS(t, now.lo) <= S4_STALE_MS) { ... S4_PITCH_DDEG(t) ... }
	} */
IMPORT ER s4_get_terrain(S4_TERRAIN *out);

/* Treat the snapshot as stale if it is older than this. */
#define S4_STALE_MS		100

/* ---------------------------------------------------------------------
 *  IF-07: terrain events (event flag owned by S4)
 *  Event bits are SET by S4 and stay set until a consumer clears them, so
 *  a tilt or collision can never be lost the way a full queue can lose
 *  a message.  Only ONE consumer (the Vehicle Controller) should clear the
 *  event bits, e.g.
 *	tk_wai_flg(flg_terrain, S4_EVT_TILT | S4_EVT_COLLISION,
 *		   TWF_ORW | TWF_BITCLR, &ptn, TMO_POL);
 *  S4_EVT_IMU_FAULT is a LEVEL: S4 sets it while the sensor is failing and
 *  clears it on recovery.  Never clear it yourself.  The team's shared
 *  fault bitmask has a single writer (Buddy 2, C06): Buddy 2 copies this
 *  bit into it, so S4 never writes the shared bitmask directly.
 * --------------------------------------------------------------------- */
#define S4_EVT_HUMP_ENTRY	0x0001	/* started climbing a hump              */
#define S4_EVT_HUMP_DONE	0x0002	/* finished a hump: peak in IF-06       */
#define S4_EVT_TILT		0x0004	/* abnormal tilt (> 25 deg) off a hump  */
#define S4_EVT_COLLISION	0x0008	/* sudden impact (> 0.8 g spike)        */
#define S4_EVT_IMU_FAULT	0x0010	/* level: sensor not answering          */

IMPORT ID	flg_terrain;

/* ---------------------------------------------------------------------
 *  Temporary, until subsystem 2 publishes its wheel data (contract C04):
 *  hump detection needs to know the car is moving.  0 = not moving (hump
 *  detection off; pitch, roll, tilt and collision still work).  Set to
 *  e.g. 100 to test humps by hand on the bench.
 * --------------------------------------------------------------------- */
#define S4_PRETEND_SPEED_MM_S	0

/* ---------------------------------------------------------------------
 *  LEGACY IF-03 (template example): subsystem 3's example consumer still
 *  sends text lines here.  Kept ONLY so the build works while subsystem 3
 *  still has its example code; terrain_task just empties the buffer.
 *  Remove together with subsystem 3's example.
 * --------------------------------------------------------------------- */
#define S4_SUMMARY_MAX		48
#define FLG_BATCH		0x0002	/* set by subsystem 3's example */

IMPORT ID	mbf_summary;
IMPORT ID	flg_events;

/* ---------------------------------------------------------------------
 *  Private to subsystem 4 (also read by subsystem4_test.c)
 * --------------------------------------------------------------------- */
typedef struct {
	UW	cycles;			/* terrain_task cycles run               */
	UW	overruns;		/* cycles still running at the next tick */
	UW	exec_max_us;		/* longest read + process time (wall clock,
					   includes being pre-empted)            */
	UW	read_max_us;		/* longest gy511_read() alone            */
	UW	logic_max_us;		/* longest imu_logic_update() alone      */
	UW	print_max_us;		/* longest status print (USB console)    */
	UW	slow_cycles;		/* read + process over 5 ms              */
	UW	jitter_max_us;		/* longest delay from tick to task start */
	UW	mtx_timeouts;		/* snapshot updates skipped (mutex busy) */
	BOOL	sensor_found;
	INT	accel_mag_mg;		/* |a| in milli-g (1000 at rest)         */
} S4_DIAG;

IMPORT S4_DIAG	s4_diag;
IMPORT ID	mtx_terrain;
IMPORT ID	cyc_terrain;
IMPORT ID	tid_terrain;
IMPORT void	terrain_task(INT stacd, void *exinf);
IMPORT void	terrain_cyclic_handler(void *exinf);

#endif /* SUBSYSTEM4_H */
