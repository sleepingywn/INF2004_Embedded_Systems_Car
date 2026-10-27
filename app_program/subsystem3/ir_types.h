/*
 *	ir_types.h
 *	Subsystem 3 -- IR line tracing and barcode sensing (Buddy 3, 2500445).
 *
 *	Constants and minimum-bit message formats for the IR subsystem.
 *	No kernel or hardware includes, so the same file is used by the
 *	micro T-Kernel build and by the Pico SDK bench test
 *	(testing/subsystem3/ir_line_bench).
 *
 *	Tribunal 1 (Imm) decisions applied here:
 *	  - Double-line proportional tracing (Live Decision Change Log, "No change").
 *	  - Buddy 3 only sends what the IR sensors detect; Buddy 2 decodes the
 *	    barcode and maps it to an instruction ("Reassigned").
 *	  - Minimum-bit representation: each field uses the fewest bits that
 *	    cover its valid range (Constraint Injection, Buddy 3 row).
 */

#ifndef IR_TYPES_H
#define IR_TYPES_H

#include <stdint.h>

/* ---------------------------------------------------------------------
 *  Sensor roles (GP26/27/28 = ADC0/1/2, analog A0 mode, D0 unconnected).
 *  LINE_A must be the LEFT-hand line sensor, otherwise the sign of the
 *  line error flips.
 * --------------------------------------------------------------------- */
#define IR_NUM_CH		3u
#define IR_CH_BARCODE		0u	/* IR Left   - GP26 / ADC0 */
#define IR_CH_LINE_A		1u	/* IR Centre - GP27 / ADC1 (left-hand)  */
#define IR_CH_LINE_B		2u	/* IR Right  - GP28 / ADC2 (right-hand); VBAT jumper OPEN */

/* Measured in S3-U02/S3-U03 (2026-10-27): the A0 output is HIGHER on black. */
#define IR_BLACK_IS_HIGH	1

/* Calibration: minimum black-white difference in raw 12-bit counts (TBC).
   Measured contrast was about 3200 counts on GP26/GP27. */
#define IR_CAL_MIN_CONTRAST	600u

/* ---------------------------------------------------------------------
 *  Line sensor placement (Buddy 3 doc section 4.1):
 *    EDGE    - each sensor over one edge of the line; centred = A ~ B ~ 50.
 *              e = 100*(A-B)/(A+B); both white = line lost.
 *    OUTSIDE - both sensors on white, one each side of the line;
 *              centred = A ~ B ~ 0.  e = A-B; both white = normal
 *              (going straight), LOST only after LINE_UNSEEN_MS.
 *  OUTSIDE selected 2026-10-27: on the car the sensor eyes cannot be
 *  brought closer than the line width (sliding test showed a gap where
 *  both read white), so EDGE gave almost only +/-100.
 * --------------------------------------------------------------------- */
#define LINE_MODE_EDGE		0
#define LINE_MODE_OUTSIDE	1
#ifndef LINE_MODE			/* can be overridden with -DLINE_MODE=0 for testing */
#define LINE_MODE		LINE_MODE_OUTSIDE
#endif

/* ---------------------------------------------------------------------
 *  Line tracing thresholds on the normalised 0 (white) .. 100 (black)
 *  scale.  All TBC on the real track.
 * --------------------------------------------------------------------- */
#define LINE_ON_BLACK		75u	/* sensor counts as "on black" at or above   */
#define LINE_OFF_WHITE		15u	/* sensor counts as "on white" at or below   */
#define LINE_LOST_TIMEOUT_MS	200u	/* EDGE: both white this long -> LINE_LOST   */
#define LINE_UNSEEN_MS		2000u	/* OUTSIDE: neither sensor saw the line this long -> LINE_LOST */

/* ---------------------------------------------------------------------
 *  Barcode capture (barcode sensor only).  Widths are counted in samples;
 *  at the 1 kHz sample rate one sample = 1 ms.  All TBC.
 * --------------------------------------------------------------------- */
#define BC_MAX_ELEMS		63u	/* fits the 6-bit count                       */
#define BC_EXPECTED_ELEMS	29u	/* Code 39 "*X*": 3 x 9 elements + 2 gaps (assumed); 0 = disable early finish */
#define BC_MIN_ELEMS		5u	/* shorter captures are treated as noise      */
#define BC_BAR_ON		60u	/* enter "bar" at or above (hysteresis)       */
#define BC_BAR_OFF		35u	/* leave "bar" at or below                    */
#define BC_QUIET_MS		150u	/* white run this long ends a capture (< 255) */
#define BC_WIDTH_SAT		255u	/* width value meaning ">= 255 samples"       */

/* ---------------------------------------------------------------------
 *  Line state: 7 states -> 3 bits.
 * --------------------------------------------------------------------- */
typedef enum {
	LINE_CALIBRATING = 0,	/* not calibrated yet                          */
	LINE_ON,		/* following the line                          */
	LINE_EDGE_ONLY,		/* a sensor is fully on black: sharp correction */
	LINE_LOST_HOLD,		/* EDGE only: both white, shorter than LINE_LOST_TIMEOUT_MS */
	LINE_LOST,		/* line not seen for the timeout               */
	LINE_BOTH_BLACK,	/* both black: junction candidate              */
	LINE_FAULT		/* IR fault (calibration failed, stale samples) */
} line_state_t;

/* ---------------------------------------------------------------------
 *  Line message, S3 -> Vehicle Controller, every 10 ms.  32 bits.
 *
 *  err is the line-position estimate ("margin of error" for proportional
 *  tracing): -100 = line far to the RIGHT, 0 = centred, +100 = line far
 *  to the LEFT.  201 values -> 8 signed bits.  Tribunal 1 proposed a
 *  9-bit angle "if the algorithm requires 0-359 degrees"; two sensors
 *  side by side measure a lateral offset, not an angle, so 8 bits is the
 *  minimum for this algorithm.
 * --------------------------------------------------------------------- */
typedef struct {
	int32_t		err		: 8;	/* -100..+100, + = line to the left  */
	uint32_t	state		: 3;	/* line_state_t                      */
	uint32_t	seq		: 4;	/* wraps; consumer detects missed msgs */
	uint32_t	fault		: 1;	/* IR fault                          */
	uint32_t	bc_active	: 1;	/* barcode capture in progress: hold speed constant */
	uint32_t	t_ms		: 15;	/* sample time in ms, wraps every 32.7 s */
} line_msg_t;

/* ---------------------------------------------------------------------
 *  Barcode message, S3 -> Vehicle Controller, sent once when a barcode
 *  ends.  It carries ONLY what the barcode sensor detected: the length of
 *  each bar and space in the order they were crossed, bar first.
 *  Classifying narrow/wide and decoding the character is Buddy 2's job.
 *
 *  Header 16 bits + count bytes of widths.
 * --------------------------------------------------------------------- */
typedef struct {
	uint16_t	count		: 6;	/* elements stored (bar first)               */
	uint16_t	overflow	: 1;	/* more than BC_MAX_ELEMS seen: do not decode */
	uint16_t	seq		: 4;	/* wraps                                     */
	uint16_t	reserved	: 5;
} bc_hdr_t;

typedef struct {
	bc_hdr_t	hdr;
	uint8_t		widths[BC_MAX_ELEMS];	/* samples per element, saturating at 255 */
} bc_msg_t;

/* Calibration for one channel: two 12-bit ADC values packed in 24 bits. */
typedef struct {
	uint32_t	white	: 12;
	uint32_t	black	: 12;
} ir_cal_t;

_Static_assert(IR_CH_LINE_A != IR_CH_LINE_B && IR_CH_LINE_A != IR_CH_BARCODE &&
	       IR_CH_LINE_B != IR_CH_BARCODE, "IR sensor roles must be distinct");
_Static_assert(sizeof(line_msg_t) == 4, "line_msg_t must be 32 bits");
_Static_assert(sizeof(bc_hdr_t) == 2, "bc_hdr_t must be 16 bits");
_Static_assert(BC_QUIET_MS < 255u, "BC_QUIET_MS must fit the 8-bit run counter");
_Static_assert(LINE_OFF_WHITE < LINE_ON_BLACK, "white threshold must be below black");
_Static_assert(BC_BAR_OFF < BC_BAR_ON, "barcode hysteresis thresholds reversed");

#endif /* IR_TYPES_H */
