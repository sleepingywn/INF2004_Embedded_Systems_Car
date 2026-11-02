/*
 *	int_console.c
 *	Integration test console: live status of S2 (motion), S3 (IR) and S4
 *	(terrain), plus single-key commands from the USB serial monitor.
 *
 *	Development tool only.  It uses the public APIs of the subsystems and
 *	keeps their safety rules: motor pulses are Buddy 2's MOTION_BENCH
 *	requests (at most 20 % power for 0.5 s), refused while a fault is
 *	latched, and IR calibration is Buddy 3's BEGIN/FINISH command.
 *
 *	Keys (also printed by 'h'):
 *	  h  help                       p  console status line on/off
 *	  t  terrain status line on/off i  IR calibration values
 *	  c  IR calibration BEGIN       f  IR calibration FINISH
 *	  o  clear the IR fault (motor tests without IR), then press r
 *	  r  motion RESET               x  motion STOP
 *	  w  both wheels  +20 % 0.5 s   s  both wheels  -20 % 0.5 s
 *	  a  left wheel   +20 % 0.5 s   d  right wheel  +20 % 0.5 s
 *	  A  left wheel   -20 % 0.5 s   D  right wheel  -20 % 0.5 s
 *	  g  drive forward (power and time below)   b  drive backward
 *	  +  -  drive power +/- 5 %                 [  ]  drive time -/+ 0.5 s
 *	  q  spin left on the spot   e  spin right  (same power and time)
 *	  <  >  drive trim -/+ 5 %: extra power for the RIGHT motor on g / b
 *	        (open-loop balance; the left motor is ~20 % stronger)
 *	  z  zero the terrain heading (hdg) for a turn-rate test
 *	  l  fast line monitor on/off (line error and state every 100 ms)
 *	  n  new run: clear the highest hump and hump count (B4-FR10)
 *	  v  print the terrain event records: time, count, value (B4-FR9)
 *	  Every barcode the IR sensor captures is printed with its decoded letter.
 */

#include "int_console.h"

#if APP_CONSOLE

#include "../subsystem2/motion.h"
#include "../subsystem2/motion_logic.h"	/* motion_barcode_decode */
#include "../subsystem3/ir_runtime.h"
#include "../subsystem4/subsystem4.h"

IMPORT INT tm_usb_console_getc(void);	/* lib/libtm usb_console.c */

#define BENCH_DUTY	200	/* permille = 20 %, Buddy 2's bench maximum */
#define BENCH_MS	500	/* Buddy 2's bench maximum                  */

LOCAL BOOL	print_on = TRUE;
LOCAL BOOL	line_fast = FALSE;	/* l: line monitor every 100 ms     */
LOCAL UW	barcodes_seen;		/* barcode captures already printed */

LOCAL const char *const line_state_name[8] = {
	"CALIBRATING", "ON", "EDGE_ONLY", "LOST_HOLD", "LOST", "BOTH_BLACK", "FAULT", "?"
};
LOCAL INT	drive_pct = 35;		/* g / b power, percent             */
LOCAL INT	drive_ms  = 2000;	/* g / b time                       */
LOCAL INT	drive_trim = 20;	/* g / b: right motor gets this % more.
					   Measured 2026-10-31 at 15 %: left
					   wheel went 1.18-1.23x further.   */
LOCAL UW	next_id = 1;	/* request ids only ever increase */

LOCAL const char *const motion_state_name[8] = {
	"IDLE", "DRIVING", "TRAVELLING", "TURNING",
	"TRACING", "REACQUIRE", "ERROR", "PULSING"
};
LOCAL const char *const motion_result_name[8] = {
	"NONE", "ACCEPTED", "COMPLETED", "CANCELLED",
	"EXPIRED", "FAILED", "REJECTED", "?"
};
LOCAL const char *const ir_state_name[4] = {
	"UNCALIBRATED", "CALIBRATING", "READY", "FAULT"
};
LOCAL const char *const motion_fault_name[8] = {
	"encoder", "stall", "timeout", "ir", "handoff", "range", "external", "hardware"
};

/* The subsystems' status calls only poll their mutex (TMO_POL), so a call
   can fail with E_TMOUT just because the owner task holds it right now.
   Retry a few times before giving up. */
LOCAL ER get_motion(motion_status_t *m)
{
	ER	er = E_TMOUT;
	INT	i;

	for (i = 0; i < 20 && er == E_TMOUT; i++) {
		er = motion_get_status(m);
		if (er == E_TMOUT) tk_dly_tsk(2);
	}
	return er;
}

LOCAL ER get_ir(ir_runtime_status_t *ir)
{
	ER	er = E_TMOUT;
	INT	i;

	for (i = 0; i < 20 && er == E_TMOUT; i++) {
		er = ir_runtime_get_status(ir);
		if (er == E_TMOUT) tk_dly_tsk(2);
	}
	return er;
}

LOCAL void print_help(void)
{
	tm_printf((UB*)
		"\n[con] keys:\n"
		"[con]  h help    p status line on/off    t terrain line on/off\n"
		"[con]  c IR calibration BEGIN   f IR calibration FINISH   i IR calibration values\n"
		"[con]  o clear the IR fault (motor test without IR), then r\n"
		"[con]  r motion RESET           x motion STOP\n"
		"[con]  w both +20%%  s both -20%%  a left +20%%  d right +20%%  A left -20%%  D right -20%%\n"
		"[con]  (each pulse lasts 0.5 s; press r first if motion shows a fault)\n"
		"[con]  g drive forward   b drive backward   + - power   [ ] time\n"
		"[con]  q spin left   e spin right   z zero heading (hdg on the terrain line)\n"
		"[con]  l fast line monitor (line error/state every 100 ms); barcodes print by themselves\n"
		"[con]  < > drive trim: right motor -/+ 5 %% (car pulls right -> press >)\n"
		"[con]  n new run (clear highest hump)   v terrain event records\n"
		"[con]  (drive keys use the measured motor directions; x stops)\n\n");
}

LOCAL void print_faults(UINT faults)
{
	UINT	i;

	if (faults == 0) {
		tm_printf((UB*)"none");
		return;
	}
	for (i = 0; i < 8; i++) {
		if (faults & (1U << i)) tm_printf((UB*)"%s ", motion_fault_name[i]);
	}
}

LOCAL void print_status(void)
{
	ir_runtime_status_t	ir;
	motion_status_t		m;
	ER			er;

	er = get_ir(&ir);
	if (er == E_OK) {
		tm_printf((UB*)"[con] IR %s%s raw bc %4u L %4u R %4u",
			  ir_state_name[ir.state & 3], ir.healthy ? " ok" : " FAULT",
			  (UINT)ir.raw.barcode, (UINT)ir.raw.line_a, (UINT)ir.raw.line_b);
		if (ir.state == IR_RUNTIME_READY) {
			tm_printf((UB*)" | norm %3u %3u %3u",
				  (UINT)ir.normalized[0].value, (UINT)ir.normalized[1].value,
				  (UINT)ir.normalized[2].value);
		}
		if (ir.failed_channels) {
			tm_printf((UB*)" | failed%s%s%s",
				  (ir.failed_channels & 1) ? " GP26" : "",
				  (ir.failed_channels & 2) ? " GP27" : "",
				  (ir.failed_channels & 4) ? " GP28" : "");
		}
		{
			line_msg_t	ln;
			uint32_t	ln_ms;
			if (motion_debug_line(&ln, &ln_ms) == E_OK) {
				tm_printf((UB*)" | line err %+d %s",
					  (INT)ln.err, line_state_name[ln.state & 7]);
			}
		}
		tm_printf((UB*)" | acq %s n %u late %u us scan %u us flt %u",
			  ir.acquisition.running ? "on" : "STOPPED",
			  (UINT)ir.acquisition.samples,
			  (UINT)ir.acquisition.max_lateness_us,
			  (UINT)ir.acquisition.max_scan_us,
			  (UINT)ir.acquisition.faults);
	} else {
		tm_printf((UB*)"[con] IR status busy (%d)", (INT)er);
	}

	er = get_motion(&m);
	if (er == E_OK) {
		tm_printf((UB*)"\n[con] MOTION %s last %s id %u | faults ",
			  motion_state_name[m.state & 7], motion_result_name[m.result & 7],
			  (UINT)m.request_id);
		print_faults(m.faults);
		tm_printf((UB*)"| duty L %+d R %+d | enc L %d R %d bad %u\n",
			  (INT)motion_left_duty(&m), (INT)motion_right_duty(&m),
			  (INT)m.left_count, (INT)m.right_count, (UINT)m.invalid_edges);
	} else {
		tm_printf((UB*)"\n[con] motion status busy (%d)\n", (INT)er);
	}
}

LOCAL void print_ir_calibration(void)
{
	ir_runtime_status_t	ir;
	UINT			i;
	static const char *const pin[3] = { "GP26 barcode", "GP27 line L ", "GP28 line R " };

	if (get_ir(&ir) != E_OK) {
		tm_printf((UB*)"[con] IR status busy, try again\n");
		return;
	}
	tm_printf((UB*)"[con] IR %s, %u calibration samples\n",
		  ir_state_name[ir.state & 3], (UINT)ir.calibration_samples);
	for (i = 0; i < IR_NUM_CH; i++) {
		INT w = ir.calibration[i].white, b = ir.calibration[i].black;
		tm_printf((UB*)"[con]   %s white %4d black %4d contrast %4d%s\n",
			  pin[i], w, b, b - w,
			  (ir.failed_channels & (1U << i)) ? "  FAILED (needs 600)" : "");
	}
}

/* Submit one motion request with a fresh id and the current epoch. */
LOCAL void submit(motion_command_t cmd, INT left, INT right, UW ttl_ms, const char *what)
{
	motion_status_t		m;
	motion_request_t	req;
	ER			er;

	INT	i;

	er = get_motion(&m);
	if (er == E_OK) {
		er = motion_request_init(&req, cmd, MOTION_LOCAL, m.epoch, next_id++,
					 ttl_ms, 0, left, right);
	}
	for (i = 0; i < 20 && er == E_OK; i++) {
		er = motion_submit(&req);
		if (er != E_TMOUT) break;
		er = E_OK;
		tk_dly_tsk(2);
	}
	tm_printf((UB*)"[con] %s: %s (%d)\n", what, (er == E_OK) ? "sent" : "REFUSED", (INT)er);
	if (er == E_OK) {
		tk_dly_tsk(60);		/* let motion_task take it (20 ms period) */
		if (get_motion(&m) == E_OK && m.result == MOTION_REJECTED) {
			tm_printf((UB*)"[con]   motion rejected it (faults: ");
			print_faults(m.faults);
			tm_printf((UB*)"). After o, r should clear them; if r is rejected"
				  " too, an IR or other fault is still active\n");
		} else if (cmd == MOTION_RESET && get_motion(&m) == E_OK) {
			tm_printf((UB*)"[con]   reset done: motion %s, faults ",
				  motion_state_name[m.state & 7]);
			print_faults(m.faults);
			tm_printf((UB*)"\n");
		}
	}
}

LOCAL void ir_command(ir_runtime_command_t cmd)
{
	ER er = ir_runtime_calibration(cmd);

	if (cmd == IR_RUNTIME_BEGIN) {
		tm_printf((UB*)"[con] IR calibration BEGIN: %s (%d)\n",
			  (er == E_OK) ? "started" : "REFUSED", (INT)er);
		if (er == E_OK) {
			tm_printf((UB*)"[con]   hold the car still and move ALL THREE sensors over"
				  " white and over black, then press f\n");
		}
	} else {
		tm_printf((UB*)"[con] IR calibration FINISH: %s (%d)\n",
			  (er == E_OK) ? "sent" : "REFUSED (press c first)", (INT)er);
		if (er == E_OK) {
			tk_dly_tsk(200);
			print_ir_calibration();
		}
	}
}

/* l: one short line per 100 ms while sliding the car across the line */
LOCAL void print_line_fast(void)
{
	line_msg_t		ln;
	uint32_t		ln_ms;
	ir_runtime_status_t	ir;

	if (motion_debug_line(&ln, &ln_ms) != E_OK) {
		tm_printf((UB*)"[con] line: no samples yet (IR not READY? press c ... f)\n");
		return;
	}
	tm_printf((UB*)"[con] line err %+4d  %-10s", (INT)ln.err, line_state_name[ln.state & 7]);
	if (get_ir(&ir) == E_OK && ir.state == IR_RUNTIME_READY) {
		tm_printf((UB*)"  norm L %3u R %3u  bc %3u",
			  (UINT)ir.normalized[1].value, (UINT)ir.normalized[2].value,
			  (UINT)ir.normalized[0].value);
	}
	tm_printf((UB*)"%s\n", ln.bc_active ? "  [barcode capture]" : "");
}

/* v: B4-FR9 event records */
LOCAL void print_events_table(void)
{
	static const char *const name[S4_EV_NUM] = {
		"HUMP_ENTRY", "HUMP_DONE", "TILT", "COLLISION", "IMU_FAULT"
	};
	static const char *const unit[S4_EV_NUM] = {
		"x0.1 deg", "mm", "x0.1 deg", "mg", "1=set 0=cleared"
	};
	S4_EVENTS	e;
	SYSTIM		now;
	INT		i;

	if (s4_get_events(&e) != E_OK) {
		tm_printf((UB*)"[con] events: busy, try again\n");
		return;
	}
	tk_get_tim(&now);
	tm_printf((UB*)"[con] terrain events (source %u = IMU), now %u ms:\n",
		  (UINT)e.source, (UINT)now.lo);
	for (i = 0; i < S4_EV_NUM; i++) {
		if (e.ev[i].count == 0) {
			tm_printf((UB*)"[con]   %-10s never\n", name[i]);
		} else {
			tm_printf((UB*)"[con]   %-10s x%u  last at %u ms (%u ms ago)  value %d %s\n",
				  name[i], (UINT)e.ev[i].count, (UINT)e.ev[i].time_ms,
				  (UINT)(now.lo - e.ev[i].time_ms), (INT)e.ev[i].value, unit[i]);
		}
	}
}

/* Print every new barcode capture from S3 and what Buddy 2's decoder makes of it */
LOCAL void check_barcode(void)
{
	bc_msg_t	bc;
	uint32_t	bc_ms, count;
	char		ch;
	UINT		i;

	if (motion_debug_barcode(&bc, &bc_ms, &count) != E_OK || count == barcodes_seen) return;
	barcodes_seen = count;
	ch = motion_barcode_decode(&bc);
	tm_printf((UB*)"[con] BARCODE #%u: %u elements%s -> %s",
		  (UINT)count, (UINT)bc.hdr.count, bc.hdr.overflow ? " (overflow)" : "",
		  ch ? "decoded '" : "NOT decoded");
	if (ch) tm_printf((UB*)"%c' (%s)", ch,
			  ch == 'A' ? "left" : ch == 'B' ? "right" : ch == 'C' ? "straight" :
			  ch == 'D' ? "U-turn" : "?");
	tm_printf((UB*)"\n[con]   widths ms:");
	for (i = 0; i < bc.hdr.count && i < BC_MAX_ELEMS; i++) {
		tm_printf((UB*)" %u", (UINT)bc.widths[i]);
	}
	tm_printf((UB*)"\n");
}

LOCAL void handle_key(INT c)
{
	switch (c) {
	case 'h': case '?':	print_help(); break;
	case 'p':		print_on = !print_on;
				tm_printf((UB*)"[con] status line %s\n", print_on ? "on" : "off");
				break;
	case 't':		s4_status_print = !s4_status_print;
				tm_printf((UB*)"[con] terrain line %s\n", s4_status_print ? "on" : "off");
				break;
	case 'i':		print_ir_calibration(); break;
	case 'c':		ir_command(IR_RUNTIME_BEGIN); break;
	case 'f':		ir_command(IR_RUNTIME_FINISH); break;
	case 'o':		tm_printf((UB*)"[con] IR fault cleared for a motor test (%d);"
					  " now press r\n",
					  (INT)motion_set_fault(MOTION_HEALTH_IR, false));
				break;
	case 'r':		submit(MOTION_RESET, 0, 0, 1000, "motion RESET"); break;
	case 'x':		tm_printf((UB*)"[con] motion STOP (%d)\n", (INT)motion_stop()); break;
	case 'w':		submit(MOTION_BENCH,  BENCH_DUTY,  BENCH_DUTY, BENCH_MS, "both +20%"); break;
	case 's':		submit(MOTION_BENCH, -BENCH_DUTY, -BENCH_DUTY, BENCH_MS, "both -20%"); break;
	case 'a':		submit(MOTION_BENCH,  BENCH_DUTY, 0, BENCH_MS, "left +20%"); break;
	case 'A':		submit(MOTION_BENCH, -BENCH_DUTY, 0, BENCH_MS, "left -20%"); break;
	case 'd':		submit(MOTION_BENCH, 0,  BENCH_DUTY, BENCH_MS, "right +20%"); break;
	case 'D':		submit(MOTION_BENCH, 0, -BENCH_DUTY, BENCH_MS, "right -20%"); break;
	case 'g': case 'b': {
		INT dir  = (c == 'g') ? 1 : -1;
		INT duty = drive_pct * 10;
		INT rduty = duty * (100 + drive_trim) / 100;
		if (rduty > MOTION_BENCH_DUTY_MAX) rduty = MOTION_BENCH_DUTY_MAX;
		tm_printf((UB*)"[con] drive %s %d %% (right %+d %%) for %d ms\n",
			  (c == 'g') ? "FORWARD" : "BACKWARD", drive_pct, drive_trim, drive_ms);
		submit(MOTION_BENCH, dir * MOTION_LEFT_MOTOR_SIGN * duty,
		       dir * MOTION_RIGHT_MOTOR_SIGN * rduty, (UW)drive_ms,
		       (c == 'g') ? "drive forward" : "drive backward");
		break;
	}
	case 'q': case 'e': {
		INT duty = drive_pct * 10;
		INT dir  = (c == 'q') ? 1 : -1;	/* q: left wheel back, right forward */
		tm_printf((UB*)"[con] spin %s %d %% for %d ms\n",
			  (c == 'q') ? "LEFT" : "RIGHT", drive_pct, drive_ms);
		submit(MOTION_BENCH, -dir * MOTION_LEFT_MOTOR_SIGN * duty,
		       dir * MOTION_RIGHT_MOTOR_SIGN * duty, (UW)drive_ms,
		       (c == 'q') ? "spin left" : "spin right");
		break;
	}
	case 'n':		tm_printf((UB*)"[con] new run (s4_reset_run): %d\n", (INT)s4_reset_run());
				break;
	case 'v':		print_events_table(); break;
	case 'l':		line_fast = !line_fast;
				tm_printf((UB*)"[con] fast line monitor %s\n", line_fast ? "on" : "off");
				break;
	case 'z':		s4_heading_zero_req = TRUE;
				tm_printf((UB*)"[con] heading zeroed\n"); break;
	case '<': case ',':	if (drive_trim > -50) drive_trim -= 5;
				tm_printf((UB*)"[con] drive trim: right motor %+d %%\n", drive_trim); break;
	case '>': case '.':	if (drive_trim < 50) drive_trim += 5;
				tm_printf((UB*)"[con] drive trim: right motor %+d %%\n", drive_trim); break;
	case '+': case '=':	if (drive_pct < MOTION_BENCH_DUTY_MAX / 10) drive_pct += 5;
				tm_printf((UB*)"[con] drive power %d %%\n", drive_pct); break;
	case '-': case '_':	if (drive_pct > 10) drive_pct -= 5;
				tm_printf((UB*)"[con] drive power %d %%\n", drive_pct); break;
	case ']':		if (drive_ms < (INT)MOTION_BENCH_TIME_MAX_MS) drive_ms += 500;
				tm_printf((UB*)"[con] drive time %d ms\n", drive_ms); break;
	case '[':		if (drive_ms > 500) drive_ms -= 500;
				tm_printf((UB*)"[con] drive time %d ms\n", drive_ms); break;
	case '\r': case '\n': case ' ': break;
	default:		tm_printf((UB*)"[con] unknown key '%c', press h\n", c); break;
	}
}

LOCAL void console_task(INT stacd, void *exinf)
{
	UW	elapsed = 0;
	INT	c;

	(void)stacd;
	(void)exinf;

	tk_dly_tsk(3000);	/* let the serial monitor connect */
	tm_printf((UB*)"\n[con] integration console ready\n");
	print_help();

	for (;;) {
		while ((c = tm_usb_console_getc()) >= 0) handle_key(c);
		check_barcode();
		if (line_fast) print_line_fast();

		elapsed += CONSOLE_PERIOD_MS;
		if (elapsed >= CONSOLE_PRINT_MS) {
			elapsed = 0;
			if (print_on) print_status();
		}
		tk_dly_tsk(CONSOLE_PERIOD_MS);
	}
}

EXPORT ER int_console_start(void)
{
	ID tid = app_start_task(console_task, CONSOLE_PRI, TP_PRC1, "int_console");
	return (tid < E_OK) ? tid : E_OK;
}

#else  /* !APP_CONSOLE */

IMPORT ER s1_diagnostic_start(void);
EXPORT ER int_console_start(void) { return s1_diagnostic_start(); }

#endif /* APP_CONSOLE */
