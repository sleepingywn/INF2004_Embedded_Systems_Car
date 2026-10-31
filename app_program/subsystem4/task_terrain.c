/*
 *	task_terrain.c
 *	Subsystem 4 -- terrain_task and its 20 ms cyclic handler.
 *
 *	Every 20 ms:
 *	  cyclic handler -> wakes terrain_task
 *	  terrain_task   -> read GY-511 -> imu_logic_update() -> publish
 *	                    snapshot (IF-06, under mtx_terrain) and event bits
 *	                    (IF-07, flg_terrain)
 *
 *	terrain_task is the only task that touches I2C0 (TEAM.md section 3).
 *	It also measures its own timing (C03): execution time, release jitter
 *	(tick -> task start) and overruns, printed once a second.
 */

#include "subsystem4.h"
#include "drv_gy511.h"
#include "imu_logic.h"
#include "../subsystem2/motion.h"	/* IF-09: Buddy 2's wheel encoder counts */

#define FAULT_AFTER	5	/* failed reads in a row -> IMU fault (100 ms) */
#define REINIT_EVERY	50	/* while faulted, retry the sensor every 1 s   */
#define PRINT_EVERY	50	/* status line every 50 x 20 ms = 1 s          */

EXPORT volatile BOOL s4_status_print = TRUE;	/* status line on/off */
#define MTX_WAIT_MS	2	/* never wait longer than this for the lock     */

EXPORT S4_DIAG	s4_diag;

LOCAL S4_TERRAIN	snapshot;	/* IF-06, guarded by mtx_terrain */
LOCAL imu_logic_t	logic;

/* Shared between the cyclic handler and the task */
/* Shared between the cyclic handler and the task.  Each flag is its own
   byte (the smallest unit the CPU stores in one instruction), NOT a bit in
   a shared word: packing them together would need a read-modify-write
   that the handler could interrupt (minimum-bit requirement, safe access). */
LOCAL volatile UW	release_us;	/* when the handler last fired          */
LOCAL volatile UB	busy = 1;	/* task still working on the last cycle */
LOCAL volatile UB	counting;	/* 0 while (re)initialising the sensor,
					   so that is not an overrun            */

/* Free-running 1 MHz hardware timer (RP2040 TIMERAWL), for timing only */
LOCAL UW now_us(void)
{
	return in_w(TIMER_TIMERAWL);
}

/* ---------------------------------------------------------------- */

EXPORT void terrain_cyclic_handler(void *exinf)
{
	(void)exinf;
	if (busy) {
		if (counting) s4_diag.overruns++;	/* previous cycle still running */
		return;
	}
	release_us = now_us();
	tk_wup_tsk(tid_terrain);
}

EXPORT ER s4_get_terrain(S4_TERRAIN *out)
{
	ER er = tk_loc_mtx(mtx_terrain, MTX_WAIT_MS);
	if (er < E_OK) return er;
	*out = snapshot;
	tk_unl_mtx(mtx_terrain);
	return E_OK;
}

/* ---------------------------------------------------------------- */

/* Chip X/Y/Z -> car forward / side / up.  Bench test: chip X = forward
   on our car.  The mounting angle (upside down, ~15 deg tilt) is removed
   by imu_logic's calibration, so only the forward axis must be right. */
/* ---------------------------------------------------------------------
 *  Wheel data from subsystem 2 (IF-09, motion_get_status).
 *  Uses the RAW encoder counts, which Buddy 2 publishes even before his
 *  speed controller is calibrated, with the wheel geometry from his
 *  motion_config.h.  Forward travel of the car centre = average of the
 *  two wheels.
 * --------------------------------------------------------------------- */
#define S4_MIN_MOVING_MM_S	20	/* below this the car counts as stopped */


LOCAL BOOL	wheel_have;		/* previous counts valid              */
LOCAL UW	wheel_prev_l, wheel_prev_r;
LOCAL float	wheel_speed_mm_s;	/* last forward speed (for printing)  */
LOCAL float	wheel_turn_dps;		/* last turn rate, + = left (CCW)     */
LOCAL BOOL	wheel_ok;		/* wheel data available               */
LOCAL float	wheel_heading_deg;	/* turn since boot / zero, + = left   */
LOCAL float	heading_at_print;	/* heading at the previous status line */
EXPORT volatile BOOL s4_heading_zero_req;	/* set to zero the heading    */

/* Forward distance since the previous call, in mm (negative = reversing).
   Returns 0 and clears wheel_ok when subsystem 2 has no encoder data. */
LOCAL float wheel_travel_mm(float dt)
{
	motion_status_t	m;
	INT		dl, dr;
	float		left_mm, right_mm;
	const float	mm_per_count_l = (float)MOTION_LEFT_CIRCUMFERENCE_UM * 0.001f
					/ (float)MOTION_LEFT_COUNTS_REV;
	const float	mm_per_count_r = (float)MOTION_RIGHT_CIRCUMFERENCE_UM * 0.001f
					/ (float)MOTION_RIGHT_COUNTS_REV;

	if (MOTION_LEFT_COUNTS_REV == 0 || MOTION_RIGHT_COUNTS_REV == 0 ||
	    motion_get_status(&m) != E_OK) {
		wheel_ok = FALSE;
		return 0.0f;
	}
	if (!wheel_have) {			/* first sample: just remember */
		wheel_prev_l = m.left_count;
		wheel_prev_r = m.right_count;
		wheel_have = TRUE;
		wheel_ok = TRUE;
		return 0.0f;
	}
	dl = (INT)(m.left_count  - wheel_prev_l);	/* wrap-safe */
	dr = (INT)(m.right_count - wheel_prev_r);
	wheel_prev_l = m.left_count;
	wheel_prev_r = m.right_count;

	left_mm  = (float)(dl * MOTION_LEFT_ENCODER_SIGN)  * mm_per_count_l;
	right_mm = (float)(dr * MOTION_RIGHT_ENCODER_SIGN) * mm_per_count_r;
	if (s4_heading_zero_req) {
		wheel_heading_deg = heading_at_print = 0.0f;
		s4_heading_zero_req = FALSE;
	}
	wheel_heading_deg += (right_mm - left_mm) / (float)MOTION_TRACK_WIDTH_MM * 57.29578f;
	if (dt > 0.0f) {
		wheel_speed_mm_s = 0.5f * (left_mm + right_mm) / dt;
		wheel_turn_dps   = (right_mm - left_mm) / (float)MOTION_TRACK_WIDTH_MM
				   / dt * 57.29578f;
	}
	wheel_ok = TRUE;
	return 0.5f * (left_mm + right_mm);
}

LOCAL imu_accel_t map_axes(const gy511_accel_t *r)
{
	imu_accel_t a = { r->x, r->y, r->z };
	return a;
}

LOCAL H to_tenths(float v)			/* 12.34 -> 123 */
{
	return (H)(v >= 0.0f ? v * 10.0f + 0.5f : v * 10.0f - 0.5f);
}

/* Print a value given in tenths as +12.3 (tm_printf has no %f) */
LOCAL void print_tenths(const char *label, INT t)
{
	INT a = (t < 0) ? -t : t;
	tm_printf((UB*)"%s %c%d.%d", label, (t < 0) ? '-' : '+', a / 10, a % 10);
}

LOCAL void print_events(UINT ev)
{
	if (ev & IMU_EVT_HUMP_ENTRY) {
		tm_printf((UB*)"terrain: HUMP ENTRY");
		print_tenths("  pitch", to_tenths(logic.pitch_deg));
		tm_printf((UB*)"\n");
	}
	if (ev & IMU_EVT_HUMP_DONE) {
		tm_printf((UB*)"terrain: HUMP DETECTED (%s)",
			  logic.current_hump_one_sided ? "one-sided" : "full-width");
		print_tenths("  peak mm", to_tenths(logic.current_hump_peak_mm));
		print_tenths("  peak tilt", to_tenths(logic.current_hump_peak_tilt_deg));
		print_tenths("  highest mm", to_tenths(logic.highest_hump_peak_mm));
		tm_printf((UB*)"  count %u\n", (UINT)logic.hump_count);
	}
	if (ev & IMU_EVT_TILT) {
		tm_printf((UB*)"terrain: TILT");
		print_tenths("  pitch", to_tenths(logic.pitch_deg));
		print_tenths("  roll", to_tenths(logic.roll_deg));
		tm_printf((UB*)"\n");
	}
	if (ev & IMU_EVT_COLLISION) {
		tm_printf((UB*)"terrain: COLLISION  |a| %d mg\n",
			  (INT)(logic.accel_mag_g * 1000.0f));
	}
}

LOCAL void print_status(void)
{
	tm_printf((UB*)"terrain:");
	print_tenths(" pitch", S4_PITCH_DDEG(snapshot));
	print_tenths(" roll", S4_ROLL_DDEG(snapshot));
	print_tenths(" tilt", to_tenths(logic.tilt_total_deg));
	if (wheel_ok) {
		/* turn = average over the last status interval (1 s) */
		float turn = (wheel_heading_deg - heading_at_print)
			     * (1000.0f / (float)(PRINT_EVERY * S4_PERIOD_MS));
		heading_at_print = wheel_heading_deg;
		tm_printf((UB*)"  v %d mm/s  turn %+d dps  hdg %+d deg",
			  (INT)wheel_speed_mm_s, (INT)turn, (INT)wheel_heading_deg);
	} else {
		tm_printf((UB*)"  v --");
	}
	tm_printf((UB*)" |a| %d mg  %s  humps %u  highest %u mm",
		  s4_diag.accel_mag_mg, imu_hump_state_name(logic.hump_state),
		  (UINT)snapshot.hump_count, (UINT)snapshot.high_peak_mm);
	tm_printf((UB*)"  | exec max %u us (read %u, logic %u)  slow %u  print max %u us"
		  "  jitter max %u us  overruns %u  i2c err %u%s\n",
		  s4_diag.exec_max_us, s4_diag.read_max_us, s4_diag.logic_max_us,
		  s4_diag.slow_cycles, s4_diag.print_max_us,
		  s4_diag.jitter_max_us, s4_diag.overruns,
		  gy511_error_count(), snapshot.imu_fault ? "  IMU FAULT" : "");
}

/* Pack the logic's results into the 64-bit snapshot OUTSIDE the lock,
   then copy it in with one assignment under the mutex (no field-by-field
   read-modify-write on shared data).  If the lock is busy for 2 ms, skip
   this update (the next one is only 20 ms away) rather than block. */
LOCAL void publish(BOOL fault)
{
	SYSTIM			now;
	terrain_values_t	v;
	S4_TERRAIN		packed;

	tk_get_tim(&now);
	v.pitch_deg    = logic.pitch_deg;
	v.roll_deg     = logic.roll_deg;
	v.turn_dps     = wheel_turn_dps;	/* from subsystem 2's wheel counts */
	v.turn_valid   = wheel_ok ? true : false;
	v.cur_peak_mm  = logic.current_hump_peak_mm;
	v.high_peak_mm = logic.highest_hump_peak_mm;
	v.hump_state   = (uint8_t)logic.hump_state;
	v.motion_class = 0;			/* TODO(B4-FR11): motion classification */
	v.hump_count   = logic.hump_count;
	v.calibrated   = logic.calibrated;
	v.imu_fault    = fault ? true : false;
	v.time_ms      = now.lo;
	packed = terrain_pack(&v);

	if (tk_loc_mtx(mtx_terrain, MTX_WAIT_MS) < E_OK) {
		s4_diag.mtx_timeouts++;
		return;
	}
	snapshot = packed;
	tk_unl_mtx(mtx_terrain);
}

/* LEGACY IF-03: empty subsystem 3's example text lines so its buffer
   never fills.  Remove together with subsystem 3's example. */
LOCAL void drain_legacy_if03(void)
{
	UB	text[S4_SUMMARY_MAX];
	UINT	ptn;
	while (tk_rcv_mbf(mbf_summary, text, TMO_POL) > 0) { }
	tk_wai_flg(flg_events, FLG_BATCH, TWF_ORW | TWF_BITCLR, &ptn, TMO_POL);
}

/* ---------------------------------------------------------------- */

EXPORT void terrain_task(INT stacd, void *exinf)
{
	imu_config_t	cfg;
	gy511_accel_t	raw;
	UW		start_us, prev_us, fail_run = 0;
	BOOL		fault = FALSE, announced_cal = FALSE;

	(void)stacd; (void)exinf;

	tm_printf((UB*)"terrain: task %d, priority %d, every %d ms\n",
		  tk_get_tid(), S4_PRI_TERRAIN, S4_PERIOD_MS);
	gy511_i2c_scan();

	s4_diag.sensor_found = gy511_init() ? TRUE : FALSE;
	if (s4_diag.sensor_found) {
		tm_printf((UB*)"terrain: GY-511 OK: %s at 0x%02x\n",
			  gy511_chip_name(), gy511_address());
	} else {
		tm_printf((UB*)"terrain: GY-511 not found, retrying every 1 s\n");
		fault = TRUE;
		tk_set_flg(flg_terrain, S4_EVT_IMU_FAULT);
	}

	imu_logic_default_config(&cfg);
	imu_logic_init(&logic, &cfg);
	tm_printf((UB*)"terrain: calibrating -- keep the car STILL and LEVEL for 2 s\n");
	prev_us  = now_us();
	counting = 1;
	busy     = 0;			/* from now on, accept 20 ms ticks */

	for (;;) {
		tk_slp_tsk(TMO_FEVR);			/* woken by the 20 ms tick */
		busy     = 1;
		start_us = now_us();
		s4_diag.cycles++;
		if (logic.calibrated && start_us - release_us > s4_diag.jitter_max_us) {
			s4_diag.jitter_max_us = start_us - release_us;
		}

		/* ---- read + process (this is what "exec" measures) ---- */
		if (!fault && gy511_read(&raw)) {
			UINT  ev;
			UW    read_us = now_us() - start_us, logic_start;
			float dt = (float)(start_us - prev_us) * 1e-6f;
			float dist = wheel_travel_mm(dt);

			/* Bench override: pretend to move (S4_PRETEND_SPEED_MM_S) */
			if (S4_PRETEND_SPEED_MM_S > 0) {
				dist = (float)S4_PRETEND_SPEED_MM_S * dt;
			} else if (dist < (float)S4_MIN_MOVING_MM_S * dt) {
				dist = 0.0f;		/* stopped, or reversing */
			}

			prev_us  = start_us;
			fail_run = 0;
			/* TODO: the car's own acceleration from the wheel speed
			   (only needed by the full-width hump mode). */
			logic_start = now_us();
			ev = imu_logic_update(&logic, map_axes(&raw), dt, dist, 0.0f);
			s4_diag.accel_mag_mg = (INT)(logic.accel_mag_g * 1000.0f);

			{
				UW end = now_us();
				UW exec = end - start_us, logic_us = end - logic_start;
				if (logic.calibrated) {
					if (exec > s4_diag.exec_max_us)      s4_diag.exec_max_us = exec;
					if (read_us > s4_diag.read_max_us)   s4_diag.read_max_us = read_us;
					if (logic_us > s4_diag.logic_max_us) s4_diag.logic_max_us = logic_us;
					if (exec > 5000) {
						s4_diag.slow_cycles++;
						tm_printf((UB*)"terrain: SLOW cycle %u us (read %u us, logic %u us)\n",
							  exec, read_us, logic_us);
					}
				}
			}

			if (ev & (IMU_EVT_HUMP_ENTRY | IMU_EVT_HUMP_DONE |
				  IMU_EVT_TILT | IMU_EVT_COLLISION)) {
				UINT bits = 0;
				if (ev & IMU_EVT_HUMP_ENTRY) bits |= S4_EVT_HUMP_ENTRY;
				if (ev & IMU_EVT_HUMP_DONE)  bits |= S4_EVT_HUMP_DONE;
				if (ev & IMU_EVT_TILT)       bits |= S4_EVT_TILT;
				if (ev & IMU_EVT_COLLISION)  bits |= S4_EVT_COLLISION;
				tk_set_flg(flg_terrain, bits);
				print_events(ev);
			}
			if (logic.calibrated && !announced_cal) {
				tm_printf((UB*)"terrain: calibrated");
				print_tenths("  mounting pitch", to_tenths(logic.pitch0));
				print_tenths("  roll", to_tenths(logic.roll0));
				tm_printf((UB*)"\n");
				announced_cal = TRUE;
			}
		} else if (!fault && ++fail_run >= FAULT_AFTER) {
			fault = TRUE;
			tk_set_flg(flg_terrain, S4_EVT_IMU_FAULT);
			tm_printf((UB*)"terrain: IMU FAULT (%u failed reads in a row)\n", fail_run);
		} else if (fault && s4_diag.cycles % REINIT_EVERY == 0) {
			BOOL ok;
			counting = 0;	/* init takes > 20 ms: not an overrun */
			ok = gy511_init();
			counting = 1;
			if (ok) {
				fault = FALSE;
				fail_run = 0;
				s4_diag.sensor_found = TRUE;
				prev_us = now_us();
				tk_clr_flg(flg_terrain, ~(UINT)S4_EVT_IMU_FAULT);
				tm_printf((UB*)"terrain: IMU fault cleared (%s at 0x%02x)\n",
					  gy511_chip_name(), gy511_address());
			}
		}

		publish(fault);
		drain_legacy_if03();

		if (s4_status_print && s4_diag.cycles % PRINT_EVERY == 0 && logic.calibrated) {
			UW p0 = now_us(), pt;
			print_status();
			pt = now_us() - p0;
			if (pt > s4_diag.print_max_us) s4_diag.print_max_us = pt;
		}
		busy = 0;
	}
}
