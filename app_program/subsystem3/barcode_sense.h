/*
 *	barcode_sense.h
 *	Subsystem 3 -- barcode SENSING (capture) with the barcode IR sensor.
 *
 *	Tribunal 1 reassigned barcode decoding to Buddy 2: this module only
 *	measures how long each bar and space lasts and hands the list over.
 *	It never decides narrow/wide and never knows which character it saw.
 *	(The Tribunal interface map named the file barcode_decode.c; it was
 *	renamed because it no longer decodes.)
 *
 *	No kernel or hardware calls, so it can be tested with synthetic data.
 */

#ifndef BARCODE_SENSE_H
#define BARCODE_SENSE_H

#include <stdbool.h>
#include <stdint.h>
#include "ir_types.h"

typedef struct {
	uint8_t		widths[BC_MAX_ELEMS];
	uint8_t		count;		/* elements stored                   */
	uint8_t		run;		/* current run length, saturating    */
	uint8_t		seq;		/* next message sequence number      */
	bool		active;		/* capture in progress               */
	bool		is_bar;		/* current run is a bar              */
	bool		overflow;
} bc_capture_t;

void	bc_init(bc_capture_t *c);
bool	bc_active(const bc_capture_t *c);

/* Call once per sample (1 kHz) with the barcode sensor's normalised
   reading (0 white .. 100 black).  Returns true when a barcode has ended
   and *out has been filled; send 2 + out->hdr.count bytes. */
bool	bc_feed(bc_capture_t *c, uint8_t n, bc_msg_t *out);

#endif /* BARCODE_SENSE_H */
