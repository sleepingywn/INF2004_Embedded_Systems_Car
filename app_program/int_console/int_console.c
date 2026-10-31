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
 */

#include "int_console.h"

#if APP_CONSOLE

#include "../subsystem2/motion.h"
#include "../subsystem3/ir_runtime.h"
#include "../subsystem4/subsystem4.h"

IMPORT INT tm_usb_console_getc(void);	/* lib/libtm usb_console.c */

#define BENCH_DUTY	200	/* permille = 20 %, Buddy 2's bench maximum */
#define BENCH_MS	500	/* Buddy 2's bench maximum                  */

LOCAL BOOL	print_on = TRUE;
LOCAL INT	drive_pct = 35;		/* g / b power, percent             */
LOCAL INT	drive_ms  = 2000;	/* g / b time                       */
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
		tm_printf((UB*)"[con] drive %s %d %% for %d ms\n",
			  (c == 'g') ? "FORWARD" : "BACKWARD", drive_pct, drive_ms);
		submit(MOTION_BENCH, dir * MOTION_LEFT_MOTOR_SIGN * duty,
		       dir * MOTION_RIGHT_MOTOR_SIGN * duty, (UW)drive_ms,
		       (c == 'g') ? "drive forward" : "drive backward");
		break;
	}
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

EXPORT ER int_console_start(void) { return E_OK; }

#endif /* APP_CONSOLE */
