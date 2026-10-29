/*
 * ir_line_bench.c - S3 bench test for sensor_line.c and barcode_sense.c
 *
 * Standalone Pico SDK program (not the micro T-Kernel image). It links the
 * SAME logic files the S3 task will use (app_program/subsystem3/).
 *
 * 1. Unit tests (S3-U05..S3-U11): feeds synthetic readings into the logic
 *    and prints "[TEST] <ID> PASS|FAIL", like the team's TEST=1 build.
 * 2. Live mode on the car:
 *    - 1 kHz repeating timer samples ADC0/1/2 into a lock-free ring.
 *    - Every 10 ms the main loop drains the ring: every sample goes to the
 *      barcode capture, the line sensors are averaged and passed to
 *      line_update() (the 100 Hz line period).
 *    - Prints line data as CSV and every captured barcode.
 *
 * Keys: 'c' recalibrate (3 s), 't' re-run unit tests, 'p' pause/resume line printing.
 */
#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "hardware/adc.h"
#include "hardware/sync.h"

#include "sensor_line.h"
#include "barcode_sense.h"

/* ======================= unit tests ================================ */
static int g_fail_total;

static void report(const char *id, bool ok, const char *what)
{
	printf("[TEST] %s %s  %s\n", id, ok ? "PASS" : "FAIL", what);
	if (!ok) g_fail_total++;
}

/* Feed `len` samples of value `v`. Returns true if a message came out. */
static bool feed_run(bc_capture_t *c, uint8_t v, int len, bc_msg_t *out, int *emitted_at)
{
	bool got = false;
	for (int i = 0; i < len; i++) {
		if (bc_feed(c, v, out)) {
			got = true;
			if (emitted_at) *emitted_at = i;
		}
	}
	return got;
}

static void run_unit_tests(void)
{
	bool ok;

	/* S3-U05: normalisation, both polarities, clamping */
	{
		ir_cal_t hi = { .white = 200, .black = 3200 };	/* higher on black (measured) */
		ir_cal_t lo = { .white = 3200, .black = 200 };	/* opposite polarity */
		ir_cal_t un = { .white = 1000, .black = 1000 };	/* uncalibrated */
		ok = ir_normalise(&hi, 200) == 0 && ir_normalise(&hi, 3200) == 100 &&
		     ir_normalise(&hi, 1700) == 50 && ir_normalise(&hi, 0) == 0 &&
		     ir_normalise(&hi, 4095) == 100 &&
		     ir_normalise(&lo, 3200) == 0 && ir_normalise(&lo, 200) == 100 &&
		     ir_normalise(&lo, 1700) == 50 && ir_normalise(&un, 3000) == 0;
		report("S3-U05", ok, "normalise 0..100, both polarities, clamped");
	}

	/* S3-U06: calibration flags channels without enough contrast */
	{
		ir_cal_acc_t acc;
		ir_cal_t cal[IR_NUM_CH];
		uint16_t s1[3] = { 170, 160, 150 };	/* all on white                */
		uint16_t s2[3] = { 3350, 3340, 160 };	/* BC + A on black, B stuck    */
		ir_cal_begin(&acc);
		ir_cal_add(&acc, s1);
		ir_cal_add(&acc, s2);
		uint8_t failed = ir_cal_finish(&acc, cal);
		ok = failed == (1u << IR_CH_LINE_B) &&
		     cal[IR_CH_LINE_A].white == 160 && cal[IR_CH_LINE_A].black == 3340 &&
		     ir_normalise(&cal[IR_CH_LINE_B], 165) == 0 &&	/* noise on failed ch -> 0 */
		     ir_normalise(&cal[IR_CH_LINE_B], 4095) == 0;
		report("S3-U06", ok, "calibration: stuck channel flagged and forced to 0, others accepted");
	}

	/* S3-U07: line error and states for the selected LINE_MODE */
	{
		line_tracker_t t;
		line_state_t st;
		int8_t e;
		line_init(&t);
		bool k = true;
#if LINE_MODE == LINE_MODE_OUTSIDE
		/* values taken from the 2026-10-27 sliding test on the car */
		e = line_update(&t, 2, 2, 10, &st);    k &= (e == 0   && st == LINE_ON);         /* centred */
		e = line_update(&t, 35, 2, 10, &st);   k &= (e == 33  && st == LINE_ON);         /* line drifts left */
		e = line_update(&t, 85, 1, 10, &st);   k &= (e == 84  && st == LINE_EDGE_ONLY);
		e = line_update(&t, 2, 28, 10, &st);   k &= (e == -26 && st == LINE_ON);         /* line drifts right */
		e = line_update(&t, 1, 80, 10, &st);   k &= (e == -79 && st == LINE_EDGE_ONLY);
		e = line_update(&t, 80, 80, 10, &st);  k &= (e == 0   && st == LINE_BOTH_BLACK);
		/* both white stays ON (straight) until LINE_UNSEEN_MS = 2000 ms */
		for (int i = 0; i < 199; i++) e = line_update(&t, 3, 3, 10, &st);
		k &= (e == 0 && st == LINE_ON);       /* 1990 ms */
		e = line_update(&t, 3, 3, 10, &st);    k &= (e == 0 && st == LINE_LOST);         /* 2000 ms */
		e = line_update(&t, 40, 2, 10, &st);   k &= (e == 38 && st == LINE_ON);          /* reacquired */
		e = line_update(&t, 3, 3, 10, &st);    k &= (e == 0 && st == LINE_ON);           /* timer was reset */
		report("S3-U07", k, "OUTSIDE mode: e = A-B, both white = centred, LOST after 2 s, edge, junction");
#else
		e = line_update(&t, 50, 50, 10, &st);  k &= (e == 0   && st == LINE_ON);
		e = line_update(&t, 60, 40, 10, &st);  k &= (e == 20  && st == LINE_ON);       /* line drifts left */
		e = line_update(&t, 30, 70, 10, &st);  k &= (e == -40 && st == LINE_ON);       /* line drifts right */
		e = line_update(&t, 90, 10, 10, &st);  k &= (e == 80  && st == LINE_EDGE_ONLY);
		e = line_update(&t, 80, 80, 10, &st);  k &= (e == 0   && st == LINE_BOTH_BLACK);
		/* line leaves on the left (last error +80): hold +100, then LOST after 200 ms */
		line_update(&t, 90, 10, 10, &st);
		e = line_update(&t, 5, 5, 10, &st);    k &= (e == 100 && st == LINE_LOST_HOLD);
		for (int i = 0; i < 18; i++) line_update(&t, 5, 5, 10, &st);
		k &= (st == LINE_LOST_HOLD);           /* 190 ms */
		e = line_update(&t, 5, 5, 10, &st);    k &= (e == 100 && st == LINE_LOST);   /* 200 ms */
		e = line_update(&t, 50, 50, 10, &st);  k &= (e == 0   && st == LINE_ON);      /* reacquired */
		/* line leaves on the right */
		line_update(&t, 10, 90, 10, &st);
		e = line_update(&t, 0, 0, 10, &st);    k &= (e == -100 && st == LINE_LOST_HOLD);
		/* common-mode change (hump): ratio unchanged */
		int8_t e1 = line_update(&t, 60, 40, 10, &st);
		int8_t e2 = line_update(&t, 45, 30, 10, &st);
		k &= (e1 == 20 && e2 == 20);
		report("S3-U07", k, "EDGE mode: line error sign/scale, edge, junction, lost timeout, common-mode");
#endif
	}

	/* S3-U08: a full 29-element barcode is sent as soon as it ends */
	{
		bc_capture_t c;
		bc_msg_t m;
		bc_init(&c);
		/* Code 39-like pattern: 29 elements, narrow = 10, wide = 30 samples
		   (3 mm / 9 mm at 0.3 m/s, 1 kHz). Bar first. */
		uint8_t pat[29];
		for (int i = 0; i < 29; i++) pat[i] = (i % 3 == 1) ? 30 : 10;
		bool got = feed_run(&c, 0, 50, &m, NULL);	/* white lead-in */
		int at = -1;
		for (int i = 0; i < 29 && !got; i++) {
			uint8_t v = (i % 2 == 0) ? 95 : 5;	/* even = bar */
			got = feed_run(&c, v, pat[i], &m, &at);
			/* element i completes on the first sample of element i+1 */
		}
		if (!got) got = feed_run(&c, 5, 1, &m, &at);	/* first white after last bar */
		bool k = got && at == 0 && m.hdr.count == 29 && !m.hdr.overflow;
		for (int i = 0; k && i < 29; i++) k = (m.widths[i] == pat[i]);
		k = k && !bc_active(&c);
		report("S3-U08", k, "29-element barcode: widths exact, sent on 1st sample after last bar");
	}

	/* S3-U09: a short blip (dirt) is not sent */
	{
		bc_capture_t c;
		bc_msg_t m;
		bc_init(&c);
		bool got = feed_run(&c, 95, 8, &m, NULL) | feed_run(&c, 5, 6, &m, NULL) |
			   feed_run(&c, 95, 8, &m, NULL) | feed_run(&c, 5, 300, &m, NULL);
		report("S3-U09", !got && !bc_active(&c), "3-element blip rejected as noise");
	}

	/* S3-U10: a shorter barcode ends on the quiet timeout (count < 29) */
	{
		bc_capture_t c;
		bc_msg_t m;
		bc_init(&c);
		bool got = false;
		for (int i = 0; i < 9 && !got; i++) got = feed_run(&c, (i % 2 == 0) ? 95 : 5, 12, &m, NULL);
		int at = -1;
		if (!got) got = feed_run(&c, 5, BC_QUIET_MS + 20, &m, &at);
		bool k = got && m.hdr.count == 9 && !m.hdr.overflow &&
			 at == (int)BC_QUIET_MS - 1 && m.widths[8] == 12;
		/* sample 0 of the white run closes the last bar (run = 1); the run
		   reaches BC_QUIET_MS on sample index QUIET-1 */
		report("S3-U10", k, "9 elements: sent after BC_QUIET_MS white, count=9");
	}

	/* S3-U11: a longer-than-expected pattern is cut at BC_EXPECTED_ELEMS;
	   hysteresis keeps grey noise inside one element */
	{
		bc_capture_t c;
		bc_msg_t m;
		bc_init(&c);
		bool got = false;
		for (int i = 0; i < 70 && !got; i++) got = feed_run(&c, (i % 2 == 0) ? 95 : 5, 10, &m, NULL);
		bool k1 = got && m.hdr.count == BC_EXPECTED_ELEMS && !m.hdr.overflow;

		bc_init(&c);
		got = feed_run(&c, 95, 10, &m, NULL) | feed_run(&c, 45, 5, &m, NULL) |	/* grey inside bar */
		      feed_run(&c, 95, 10, &m, NULL) | feed_run(&c, 5, 10, &m, NULL) |
		      feed_run(&c, 50, 4, &m, NULL) |	/* grey inside space */
		      feed_run(&c, 5, 6, &m, NULL);
		/* bar = 25 samples, space = 20 so far, not yet stored */
		bool k2 = !got && c.count == 1 && c.widths[0] == 25 && c.run == 20;
		report("S3-U11", k1 && k2, "long pattern cut at 29 elements; hysteresis does not split elements");
	}

	printf("[TEST] unit tests done: %d failure(s)\n\n", g_fail_total);
}

/* ======================= live mode ================================= */
#define RING_LEN	64u			/* power of two, 64 ms of samples */
typedef struct { uint16_t r[IR_NUM_CH]; } raw3_t;

static volatile raw3_t	s_ring[RING_LEN];
static volatile uint8_t	s_head, s_tail;
static volatile uint32_t s_overruns;
static repeating_timer_t s_timer;

static bool sample_cb(repeating_timer_t *t)
{
	(void)t;
	uint8_t next = (uint8_t)((s_head + 1u) & (RING_LEN - 1u));
	if (next == s_tail) {
		s_overruns++;
		return true;
	}
	for (uint ch = 0; ch < IR_NUM_CH; ch++) {
		adc_select_input(ch);
		s_ring[s_head].r[ch] = adc_read();
	}
	__compiler_memory_barrier();		/* data before index */
	s_head = next;
	return true;
}

static bool ring_pop(raw3_t *out)
{
	if (s_tail == s_head) return false;
	for (uint ch = 0; ch < IR_NUM_CH; ch++) out->r[ch] = s_ring[s_tail].r[ch];
	__compiler_memory_barrier();
	s_tail = (uint8_t)((s_tail + 1u) & (RING_LEN - 1u));
	return true;
}

static const char *state_name(line_state_t s)
{
	static const char *n[] = { "CALIB", "ON", "EDGE", "LOST_HOLD", "LOST", "BOTH_BLACK", "FAULT" };
	return (s <= LINE_FAULT) ? n[s] : "?";
}

static ir_cal_t	s_cal[IR_NUM_CH];
static uint8_t	s_cal_failed;

static void calibrate(void)
{
	ir_cal_acc_t acc;
	raw3_t s;

	printf("\nCALIBRATING for 3 s: slide the car so every sensor crosses white AND black\n");
	printf("(line sensors over the line, barcode sensor over a bar).\n");
	ir_cal_begin(&acc);
	absolute_time_t end = make_timeout_time_ms(3000);
	while (!time_reached(end)) {
		while (ring_pop(&s)) ir_cal_add(&acc, s.r);
		sleep_ms(1);
	}
	s_cal_failed = ir_cal_finish(&acc, s_cal);
	for (uint ch = 0; ch < IR_NUM_CH; ch++) {
		printf("  ch%u (GP%u): white=%4u black=%4u %s\n", ch, 26 + ch,
		       (unsigned)s_cal[ch].white, (unsigned)s_cal[ch].black,
		       (s_cal_failed & (1u << ch)) ? "LOW CONTRAST -> IR FAULT" : "ok");
	}
	printf("Press 'c' to recalibrate, 't' to re-run unit tests, 'p' to pause line output.\n\n");
	printf("Line output (5 lines/s):\n");
	printf("  BC  = barcode sensor GP26   A = left line sensor GP27   B = right line sensor GP28\n");
	printf("        (0 = white ... 100 = black)\n");
	printf("  err = line position: -100 line far RIGHT ... 0 centred ... +100 line far LEFT\n");
#if LINE_MODE == LINE_MODE_OUTSIDE
	printf("  LINE_MODE = OUTSIDE: centred = A and B both white (~0); LOST = line unseen 2 s\n");
#else
	printf("  LINE_MODE = EDGE: centred = A and B both ~50; LOST = both white 0.2 s\n");
#endif
	printf("  state = ON / EDGE / LOST_HOLD / LOST / BOTH_BLACK (junction) / FAULT\n");
	printf("  barcode = idle / CAPTURING\n");
	printf("Barcode output: widths in ms in the order crossed, bar first (B = bar, S = space).\n\n");
}

int main(void)
{
	stdio_init_all();
	/* Wait for a serial monitor (max 30 s) so the test results are seen. */
	for (int i = 0; i < 300 && !stdio_usb_connected(); i++) sleep_ms(100);
	sleep_ms(500);

	run_unit_tests();

	adc_init();
	adc_gpio_init(26); adc_gpio_init(27); adc_gpio_init(28);
	add_repeating_timer_us(-1000, sample_cb, NULL, &s_timer);	/* fixed 1 kHz */

	calibrate();

	line_tracker_t lt;
	bc_capture_t bc;
	line_init(&lt);
	bc_init(&bc);
	bool print_line = true;
	uint32_t period = 0;
	absolute_time_t next = make_timeout_time_ms(10);

	while (true) {
		sleep_until(next);
		next = delayed_by_ms(next, 10);

		int c = getchar_timeout_us(0);
		if (c == 'c') { calibrate(); line_init(&lt); bc_init(&bc); continue; }
		if (c == 't') { g_fail_total = 0; run_unit_tests(); continue; }
		if (c == 'p') print_line = !print_line;

		/* drain ~10 samples: barcode at full 1 kHz, line sensors averaged */
		raw3_t s;
		uint32_t sa = 0, sb = 0, k = 0;
		uint8_t n_bc = 0;
		bc_msg_t msg;
		while (ring_pop(&s)) {
			n_bc = ir_normalise(&s_cal[IR_CH_BARCODE], s.r[IR_CH_BARCODE]);
			if (!(s_cal_failed & (1u << IR_CH_BARCODE)) && bc_feed(&bc, n_bc, &msg)) {
				printf("\n*** BARCODE #%u: %u elements%s\n    ",
				       msg.hdr.seq, msg.hdr.count,
				       msg.hdr.overflow ? " (OVERFLOW)" : "");
				for (uint i = 0; i < msg.hdr.count; i++)
					printf("%c%u%s", (i % 2 == 0) ? 'B' : 'S', msg.widths[i],
					       (i + 1 < msg.hdr.count) ? " " : "\n\n");
			}
			sa += ir_normalise(&s_cal[IR_CH_LINE_A], s.r[IR_CH_LINE_A]);
			sb += ir_normalise(&s_cal[IR_CH_LINE_B], s.r[IR_CH_LINE_B]);
			k++;
		}

		line_state_t st;
		int8_t err = 0;
		if (k == 0 || (s_cal_failed & ((1u << IR_CH_LINE_A) | (1u << IR_CH_LINE_B)))) {
			st = LINE_FAULT;		/* no fresh samples, or line sensor not calibrated */
		} else {
			err = line_update(&lt, (uint8_t)(sa / k), (uint8_t)(sb / k), 10, &st);
		}

		/* print every 20th period (5 lines/s) so the console stays readable */
		if (print_line && (++period % 20u) == 0u) {
			printf("BC=%3u  A=%3u  B=%3u  |  err=%+4d  state=%-10s  barcode=%s\n",
			       n_bc, k ? (unsigned)(sa / k) : 0u, k ? (unsigned)(sb / k) : 0u,
			       err, state_name(st), bc_active(&bc) ? "CAPTURING" : "idle");
		}
		if (s_overruns) {
			printf("WARNING: %lu sample ring overruns\n", (unsigned long)s_overruns);
			s_overruns = 0;
		}
	}
}
