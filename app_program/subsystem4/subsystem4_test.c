/*
 *	subsystem4_test.c
 *	On-target tests for subsystem 4.  Built only with: make TEST=1
 *	Record results in testing/subsystem4/TEST_LOG.md.
 *
 *	Run with the GY-511 connected and the car STILL and LEVEL: the tests
 *	start right after boot, during and after the 2 s calibration.
 */

#include "subsystem4.h"
#include "../test_report.h"

#if APP_TEST

EXPORT void subsystem4_test(void)
{
	T_RFLG		rflg;
	T_RMTX		rmtx;
	S4_TERRAIN	t1, t2;
	UW		cycles_before;
	ER		er1, er2;

	TEST_INIT();

	/* S4-U01: kernel objects of IF-06 / IF-07 exist */
	TEST_CHECK("S4-U01",
		   tk_ref_flg(flg_terrain, &rflg) == E_OK &&
		   tk_ref_mtx(mtx_terrain, &rmtx) == E_OK,
		   "flg_terrain and mtx_terrain exist");

	/* S4-U02: the task runs every 20 ms (about 150 cycles in 3 s) */
	cycles_before = s4_diag.cycles;
	tk_dly_tsk(3000);
	TEST_CHECK("S4-U02", s4_diag.cycles - cycles_before >= 140,
		   "terrain_task runs every 20 ms");

	/* S4-U03: no cycle ran into the next one */
	TEST_CHECK("S4-U03", s4_diag.overruns == 0, "no overruns");

	/* S4-U04: the GY-511 answers on I2C0 (GP16/GP17) */
	TEST_CHECK("S4-U04", s4_diag.sensor_found, "GY-511 found on I2C0");

	/* S4-U05: read + process fits easily in the 20 ms period */
	TEST_CHECK("S4-U05", s4_diag.exec_max_us > 0 && s4_diag.exec_max_us < 5000,
		   "execution time under 5 ms");

	/* S4-U06: after calibration, a still car reads about 1 g and is level */
	er1 = s4_get_terrain(&t1);
	TEST_CHECK("S4-U06",
		   er1 == E_OK && t1.valid &&
		   s4_diag.accel_mag_mg > 950 && s4_diag.accel_mag_mg < 1050 &&
		   t1.pitch_ddeg > -20 && t1.pitch_ddeg < 20 &&
		   t1.roll_ddeg  > -20 && t1.roll_ddeg  < 20,
		   "still car: |a| ~1 g, pitch and roll within 2 deg");

	/* S4-U07: IF-06 snapshot is fresh and keeps updating */
	tk_dly_tsk(100);
	er2 = s4_get_terrain(&t2);
	TEST_CHECK("S4-U07", er1 == E_OK && er2 == E_OK &&
		   t2.timestamp_ms != t1.timestamp_ms,
		   "snapshot updates (IF-06)");

	/* S4-U08: no IMU fault while the sensor is connected */
	TEST_CHECK("S4-U08", tk_ref_flg(flg_terrain, &rflg) == E_OK &&
		   !(rflg.flgptn & S4_EVT_IMU_FAULT),
		   "no IMU fault bit (IF-07)");

	tm_printf((UB*)"[TEST] S4 timing: exec max %u us, jitter max %u us, overruns %u\n",
		  s4_diag.exec_max_us, s4_diag.jitter_max_us, s4_diag.overruns);
	TEST_SUMMARY("subsystem4");
}

#endif /* APP_TEST */
