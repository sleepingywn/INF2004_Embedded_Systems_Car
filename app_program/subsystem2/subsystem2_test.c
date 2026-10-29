/*
 *	subsystem2_test.c
 *	On-target tests for subsystem 2.  Built only with: make TEST=1
 *	Record results in testing/subsystem2/TEST_LOG.md.
 */

#include "subsystem2.h"
#include "../test_report.h"

#if APP_TEST
#include "motion_logic_cases.h"

EXPORT void subsystem2_test(void)
{
	T_RSEM	rsem;
	T_RMPF	rmpf;
	UW	seq;
	INT	v, bad = 0;

	TEST_INIT();

	/* S2-U01: every sample value is inside the documented 0..100 range */
	for (seq = 0; seq < 40; seq++) {
		v = s2_make_value(seq);
		if (v < 0 || v > 100) bad++;
	}
	TEST_CHECK("S2-U01", bad == 0, "sample values stay within 0..100");

	/* S2-U02: flow control never over- or under-counts */
	TEST_CHECK("S2-U02",
		   tk_ref_sem(sem_slots, &rsem) == E_OK &&
		   rsem.semcnt >= 0 && rsem.semcnt <= S2_POOL_COUNT &&
		   tk_ref_mpf(mpf_samples, &rmpf) == E_OK &&
		   rmpf.frbcnt <= S2_POOL_COUNT,
		   "free slots and free blocks within pool size");

    TEST_CHECK("S2-U03", motion_case_quadrature(), "quadrature direction and invalid transitions");
    TEST_CHECK("S2-U04", motion_case_wrap(), "signed deltas across counter wrap");
    TEST_CHECK("S2-U05", motion_case_pid(), "PID saturation, anti-windup and reset");
    TEST_CHECK("S2-U06", motion_case_barcodes(), "A-D both scan directions; reject malformed capture");
    TEST_CHECK("S2-U17", motion_case_packing(), "unsigned packed signed values at boundaries");
    TEST_CHECK("S2-U18", motion_case_steering(), "line-error sign and constant-mean saturation");
	TEST_SUMMARY("subsystem2");
}

#endif /* APP_TEST */
