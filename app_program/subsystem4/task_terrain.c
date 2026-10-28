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

#define FAULT_AFTER	5	/* failed reads in a row -> IMU fault (100 ms) */
#define REINIT_EVERY	50	/* while faulted, retry the sensor every 1 s   */
#define PRINT_EVERY	50	/* status line every 50 x 20 ms = 1 s          */
#define MTX_WAIT_MS	2	/* never wait longer than this for the lock     */

EXPORT S4_DIAG	s4_diag;

LOCAL S4_TERRAIN	snapshot;	/* IF-06, guarded by mtx_terrain */
LOCAL imu_logic_t	logic;

/* Shared between the cyclic handler and the task */
LOCAL volatile UW	release_us;	/* when the handler last fired          */
LOCAL volatile BOOL	busy = TRUE;	/* task still working on the last cycle */
LOCAL volatile BOOL	counting;	/* FALSE while (re)initialising the
					   sensor, so that is not an overrun    */

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
		tm_printf((UB*)"terrain: HUMP DETECTED");
		print_tenths("  peak mm", to_tenths(logic.current_hump_peak_mm));
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
	print_tenths(" pitch", snapshot.pitch_ddeg);
	print_tenths(" roll", snapshot.roll_ddeg);
	tm_printf((UB*)" |a| %d mg  %s  humps %u", s4_diag.accel_mag_mg,
		  imu_hump_state_name(logic.hump_state), (UINT)snapshot.hump_count);
	print_tenths("  highest mm", snapshot.highest_peak_dmm);
	tm_printf((UB*)"  | exec max %u us  jitter max %u us  overruns %u  i2c err %u%s\n",
		  s4_diag.exec_max_us, s4_diag.jitter_max_us, s4_diag.overruns,
		  gy511_error_count(), snapshot.imu_fault ? "  IMU FAULT" : "");
}

/* Copy the logic's results into the snapshot under the mutex.
   If the lock is busy for 2 ms, skip this update (the next one is
   only 20 ms away) rather than block the task. */
LOCAL void publish(BOOL fault)
{
	SYSTIM	now;
	tk_get_tim(&now);

	if (tk_loc_mtx(mtx_terrain, MTX_WAIT_MS) < E_OK) {
		s4_diag.mtx_timeouts++;
		return;
	}
	snapshot.valid            = (!fault && logic.calibrated) ? TRUE : FALSE;
	snapshot.imu_fault        = fault ? TRUE : FALSE;
	snapshot.pitch_ddeg       = to_tenths(logic.pitch_deg);
	snapshot.roll_ddeg        = to_tenths(logic.roll_deg);
	snapshot.hump_state       = (UB)logic.hump_state;
	snapshot.current_peak_dmm = (UH)to_tenths(logic.current_hump_peak_mm);
	snapshot.highest_peak_dmm = (UH)to_tenths(logic.highest_hump_peak_mm);
	snapshot.hump_count       = (UH)logic.hump_count;
	snapshot.timestamp_ms     = now.lo;
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
	counting = TRUE;
	busy     = FALSE;			/* from now on, accept 20 ms ticks */

	for (;;) {
		tk_slp_tsk(TMO_FEVR);			/* woken by the 20 ms tick */
		busy     = TRUE;
		start_us = now_us();
		s4_diag.cycles++;
		if (logic.calibrated && start_us - release_us > s4_diag.jitter_max_us) {
			s4_diag.jitter_max_us = start_us - release_us;
		}

		/* ---- read + process (this is what "exec" measures) ---- */
		if (!fault && gy511_read(&raw)) {
			UINT  ev;
			float dt = (float)(start_us - prev_us) * 1e-6f;
			float dist = (float)S4_PRETEND_SPEED_MM_S * dt;

			prev_us  = start_us;
			fail_run = 0;
			/* TODO(C04): dist and the car's own acceleration come from
			   subsystem 2's wheel data once that interface is agreed. */
			ev = imu_logic_update(&logic, map_axes(&raw), dt, dist, 0.0f);
			s4_diag.accel_mag_mg = (INT)(logic.accel_mag_g * 1000.0f);

			{
				UW exec = now_us() - start_us;
				if (logic.calibrated && exec > s4_diag.exec_max_us) {
					s4_diag.exec_max_us = exec;
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
			counting = FALSE;	/* init takes > 20 ms: not an overrun */
			ok = gy511_init();
			counting = TRUE;
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

		if (s4_diag.cycles % PRINT_EVERY == 0 && logic.calibrated) {
			print_status();
		}
		busy = FALSE;
	}
}
