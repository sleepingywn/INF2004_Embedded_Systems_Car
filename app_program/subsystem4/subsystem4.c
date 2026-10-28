/*
 *	subsystem4.c
 *	Subsystem 4 -- IMU / terrain: creates its objects and starts its task.
 */

#include "subsystem4.h"

EXPORT ID	flg_terrain;		/* IF-07 */
EXPORT ID	mtx_terrain;		/* guards the IF-06 snapshot */
EXPORT ID	cyc_terrain;		/* 20 ms tick for terrain_task */
EXPORT ID	tid_terrain = -1;

EXPORT ID	mbf_summary;		/* LEGACY IF-03 (subsystem 3 example) */
EXPORT ID	flg_events;		/* LEGACY IF-03 (subsystem 3 example) */

EXPORT ER subsystem4_init(void)
{
	T_CFLG	cflg = {0};
	T_CMTX	cmtx = {0};
	T_CCYC	ccyc = {0};
	T_CMBF	cmbf = {0};

	/* IF-07: event bits, several tasks may wait on them */
	cflg.flgatr  = TA_TFIFO | TA_WMUL;
	cflg.iflgptn = 0;
	flg_terrain = tk_cre_flg(&cflg);
	if (flg_terrain < E_OK) return flg_terrain;

	/* IF-06: priority inheritance, so a low-priority reader holding the
	   lock is boosted instead of delaying terrain_task */
	cmtx.mtxatr = TA_INHERIT;
	mtx_terrain = tk_cre_mtx(&cmtx);
	if (mtx_terrain < E_OK) return mtx_terrain;

	/* 20 ms tick.  Created stopped; started once the task exists. */
	ccyc.cycatr = TA_HLNG;
	ccyc.cychdr = (FP)terrain_cyclic_handler;
	ccyc.cyctim = S4_PERIOD_MS;
	ccyc.cycphs = S4_PERIOD_MS;
	cyc_terrain = tk_cre_cyc(&ccyc);
	if (cyc_terrain < E_OK) return cyc_terrain;

	/* LEGACY IF-03: kept until subsystem 3 removes its example */
	cflg.flgatr  = TA_TFIFO | TA_WMUL;
	cflg.iflgptn = 0;
	flg_events = tk_cre_flg(&cflg);
	if (flg_events < E_OK) return flg_events;
	cmbf.mbfatr = TA_TFIFO;
	cmbf.bufsz  = 4 * S4_SUMMARY_MAX;
	cmbf.maxmsz = S4_SUMMARY_MAX;
	mbf_summary = tk_cre_mbf(&cmbf);
	return (mbf_summary < E_OK) ? mbf_summary : E_OK;
}

EXPORT ER subsystem4_start(void)
{
	/* No core affinity: it is light (well under 1 ms per 20 ms) */
	tid_terrain = app_start_task(terrain_task, S4_PRI_TERRAIN, 0, "terrain_task");
	if (tid_terrain < E_OK) return tid_terrain;
	return tk_sta_cyc(cyc_terrain);
}
