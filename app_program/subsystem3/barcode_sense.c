/*
 *	barcode_sense.c
 *	Subsystem 3 -- barcode capture (run-length of bars and spaces).
 *	See barcode_sense.h.  No kernel or hardware calls.
 *
 *	How a width is measured: the sensor is sampled at a fixed rate
 *	(1 kHz), so counting consecutive black (or white) samples gives how
 *	LONG the sensor stayed over that bar (or space).  At a constant speed
 *	that time is proportional to the printed width.  Code 39 is decoded
 *	from the RATIO of wide to narrow elements, so the absolute speed does
 *	not need to be known -- only roughly constant while the barcode is
 *	crossed.  line_msg_t.bc_active tells the Vehicle Controller to hold
 *	the speed steady during a capture.
 */

#include "barcode_sense.h"

static void bc_push(bc_capture_t *c, bc_width_t w)
{
	if (c->count < BC_MAX_ELEMS) {
		c->widths[c->count++] = w;
	} else {
		c->overflow = true;
	}
}

/* End the capture.  Returns true if *out holds a message to send. */
static bool bc_finish(bc_capture_t *c, bc_msg_t *out)
{
	bool send = (c->count >= BC_MIN_ELEMS) || c->overflow;

	if (send) {
		out->hdr.count    = c->count & 0x3Fu;	/* <= BC_MAX_ELEMS (63) */
		out->hdr.overflow = c->overflow;
		out->hdr.seq      = c->seq & 0x0Fu;
		out->hdr.reserved = 0;
		for (uint8_t i = 0; i < c->count; i++) {
			out->widths[i] = c->widths[i];
		}
		c->seq = (uint8_t)((c->seq + 1u) & 0x0Fu);
	}
	/* else: too short, treated as noise (dirt, a scratch) */

	c->active = false;
	c->is_bar = false;
	return send;
}

void bc_init(bc_capture_t *c)
{
	c->count = 0;
	c->run = 0;
	c->seq = 0;
	c->active = false;
	c->is_bar = false;
	c->overflow = false;
}

bool bc_active(const bc_capture_t *c)
{
	return c->active;
}

bool bc_feed(bc_capture_t *c, uint8_t n, bc_msg_t *out)
{
	/* Hysteresis: a grey value between BC_BAR_OFF and BC_BAR_ON keeps the
	   current colour, so noise on an edge does not split an element. */
	bool bar = c->is_bar ? (n > BC_BAR_OFF) : (n >= BC_BAR_ON);

	if (!c->active) {
		/* The barcode strip is white except for the barcode, so the
		   first bar seen while idle starts a capture. */
		if (bar) {
			c->active = true;
			c->is_bar = true;
			c->count = 0;
			c->overflow = false;
			c->run = 1;
		}
		return false;
	}

	if (bar == c->is_bar) {			/* same colour: extend the run */
		if (c->run < BC_WIDTH_SAT) c->run++;
		if (!bar && c->run >= BC_QUIET_SAMPLES) {
			/* long white after the last bar: barcode is over.
			   The trailing white is not stored. */
			return bc_finish(c, out);
		}
		return false;
	}

	/* Colour changed: the previous element is complete. */
	bc_push(c, c->run);
	c->is_bar = bar;
	c->run = 1;

	if (BC_EXPECTED_ELEMS != 0u && c->count == BC_EXPECTED_ELEMS && !c->overflow) {
		/* Last bar of the stop character finished: send now rather than
		   waiting BC_QUIET_MS (the brief asks for immediate action). */
		return bc_finish(c, out);
	}
	return false;
}

#if IR_BARCODE_DIGITAL
#include "barcode_edges.h"
static bc_edge_t edge_ring[BC_EDGE_RING];
static volatile uint32_t edge_head, edge_tail, edge_dropped, edge_position;
void bc_edge_position(uint32_t position) { edge_position = position; }
void bc_edge_irq(uint32_t us, uint32_t position, uint32_t invalid, bool black, bool ambiguous)
{
    uint32_t next = (edge_head + 1u) % BC_EDGE_RING;
    if (ambiguous || next == edge_tail) { ++edge_dropped; return; }
    edge_ring[edge_head] = (bc_edge_t){us, position, invalid, black};
    __sync_synchronize();
    edge_head = next;
}
void bc_edges_reset(bc_edges_t *c)
{
    *c = (bc_edges_t){0};
    edge_tail = edge_head;
    __sync_synchronize();
    c->dropped = edge_dropped;
}
static void edge_accept(bc_edges_t *c, bc_edge_t e)
{
    if (e.black == c->stable.black) return;
    if (!c->active && e.black) {
        c->active = true;
        c->message = (bc_msg_t){0};
        c->distance_ok = true;
    } else if (c->active) {
        unsigned n = c->message.hdr.count;
        uint32_t us = e.us - c->stable.us;
        int32_t counts = (int32_t)(e.position - c->stable.position);
        if (n == BC_MAX_ELEMS || us / 1000u > BC_WIDTH_SAT) {
            c->message.hdr.overflow = 1;
        } else {
            /* Decode ratios in ms, using original edge times, not filter delay. */
            c->message.widths[n] = (bc_width_t)((us + 500u) / 1000u);
            if (counts < 2 || counts > (int32_t)BC_WIDTH_SAT || e.invalid != c->stable.invalid)
                c->distance_ok = false;
            c->message.distance[n] = counts > 0 && counts <= (int32_t)BC_WIDTH_SAT ? (bc_width_t)counts : 0;
            c->message.hdr.count = n + 1u;
        }
    }
    c->stable = e;
    c->last_edge_us = e.us;
}
static void edge_raw(bc_edges_t *c, bc_edge_t e)
{
    if (c->pending && (uint32_t)(e.us - c->candidate.us) >= BC_GLITCH_US) {
        edge_accept(c, c->candidate);
        c->pending = false;
    }
    if (e.black == c->stable.black) {
        if (c->pending) ++c->glitches;
        c->pending = false;
    } else if (!c->pending) {
        c->candidate = e;
        c->pending = true;
    }
}
bool bc_edges_poll(bc_edges_t *c, uint32_t now, bool black, bc_msg_t *out)
{
    if (!c->primed) {
        bc_edges_reset(c);
        c->primed = true;
        c->stable = (bc_edge_t){now, edge_position, 0, black};
        c->last_move_us = now;
        c->last_position = edge_position;
        /* Starting over black is deliberately not a complete leading edge. */
        return false;
    }
    if (edge_dropped != c->dropped) {
        c->message.hdr.overflow = 1;
        c->message.hdr.reserved = 1; /* lost/ambiguous edges */
        c->message.dropped = edge_dropped - c->dropped;
        *out = c->message;
        bc_edges_reset(c); /* require resynchronisation; never decode lost edges */
        return true;
    }
    for (unsigned n = 0; n < BC_EDGE_RING && edge_tail != edge_head; ++n) {
        __sync_synchronize();
        bc_edge_t e = edge_ring[edge_tail];
        if ((int32_t)(now - e.us) < 0) break;
        __sync_synchronize();
        edge_tail = (edge_tail + 1u) % BC_EDGE_RING;
        edge_raw(c, e);
    }
    if (c->pending && (uint32_t)(now - c->candidate.us) >= BC_GLITCH_US) {
        edge_accept(c, c->candidate);
        c->pending = false;
    }
    uint32_t position = edge_position;
    if (position != c->last_position) { c->last_move_us = now; c->last_position = position; }
    if (!c->active || c->pending) return false;
    uint32_t quiet = now - c->last_edge_us;
    uint32_t narrow = UINT32_MAX;
    for (unsigned i = 0; i < c->message.hdr.count; ++i)
        if (c->message.distance[i] < narrow) narrow = c->message.distance[i];
    uint32_t quiet_counts = narrow < UINT32_MAX / 10u ? narrow * 10u : BC_QUIET_COUNTS;
    if (quiet_counts < BC_QUIET_COUNTS) quiet_counts = BC_QUIET_COUNTS;
    bool distance_end = c->distance_ok && c->message.hdr.count && !c->stable.black &&
        (int32_t)(position - c->stable.position) >= (int32_t)quiet_counts;
    bool time_end = !c->stable.black && !c->distance_ok && quiet >= BC_EDGE_QUIET_US;
    bool stalled = quiet >= BC_EDGE_STALL_US && now - c->last_move_us >= BC_EDGE_STALL_US;
    if (!distance_end && !time_end && !stalled) return false;
    c->message.distance_valid = c->distance_ok && c->message.hdr.count != 0;
    c->message.glitches = c->glitches;
    if (c->stable.black) { c->message.hdr.overflow = 1; c->message.hdr.reserved = 2; }
    else if (c->message.hdr.count != 29u) c->message.hdr.reserved = 3;
    *out = c->message;
    c->active = false;
    c->message = (bc_msg_t){0};
    c->glitches = 0;
    return true;
}
#endif
