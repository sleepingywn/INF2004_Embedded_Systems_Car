/*
 *  barcode_sense.c
 *  Subsystem 3 -- barcode capture (run-length of bars and spaces).
 *  See barcode_sense.h.  No kernel or hardware calls.
 *
 *  How a width is measured: the sensor is sampled at a fixed rate
 *  (1 kHz), so counting consecutive black (or white) samples gives how
 *  LONG the sensor stayed over that bar (or space).  At a constant speed
 *  that time is proportional to the printed width.  Code 39 is decoded
 *  from the RATIO of wide to narrow elements, so the absolute speed does
 *  not need to be known -- only roughly constant while the barcode is
 *  crossed.  line_msg_t.bc_active tells the Vehicle Controller to hold
 *  the speed steady during a capture.
 */

#include "barcode_sense.h"

static void bc_push(bc_capture_t *c, bc_width_t w)
{
    if (c->count < BC_MAX_ELEMS)
    {
        c->widths[c->count++] = w;
    }
    else
    {
        c->overflow = true;
    }
}

/* End the capture.  Returns true if *out holds a message to send. */
static bool bc_finish(bc_capture_t *c, bc_msg_t *out)
{
    bool send = (c->count >= BC_MIN_ELEMS) || c->overflow;

    if (send)
    {
        out->hdr.count = c->count & 0x3Fu; /* <= BC_MAX_ELEMS (63) */
        out->hdr.overflow = c->overflow;
        out->hdr.seq = c->seq & 0x0Fu;
        out->hdr.reserved = 0;
        for (uint8_t i = 0; i < c->count; i++)
        {
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
    c->widest = 0;
}

bool bc_active(const bc_capture_t *c)
{
    return c->active;
}

/* White this long after the last bar ends the capture.  Route profile: at
 * least BC_QUIET_MS, and at least BC_QUIET_WIDEST x the longest element of
 * this barcode, capped at BC_QUIET_MAX_MS.  Code 39 needs a quiet zone of
 * >= 10 narrow widths, which is >= ~3.5 of its wide elements, so a white run
 * 4x the widest element is past the barcode.  Car 25 Nov 2026: a bigger
 * printed barcode driven at V5 speed had wide gaps up to 1483 ms, and the
 * fixed 1.5 s ended both captures early (21 and 19 of 29 elements). */
static uint32_t bc_quiet_samples(const bc_capture_t *c)
{
#ifdef BARCODE_WIDE
    /* A lone stripe longer than the maximum quiet window can be track
       tape at a circle exit. Let a normal white margin discard it before
       it becomes a false prefix on the next label. */
    if (c->count == 1u && c->widest >= BC_QUIET_MAX_SAMPLES)
    {
        return BC_QUIET_SAMPLES;
    }
    uint32_t q = (uint32_t)c->widest * BC_QUIET_WIDEST;
    if (q < BC_QUIET_SAMPLES)
    {
        q = BC_QUIET_SAMPLES;
    }
    if (q > BC_QUIET_MAX_SAMPLES)
    {
        q = BC_QUIET_MAX_SAMPLES;
    }
    return q;
#else
    (void)c;
    return BC_QUIET_SAMPLES;
#endif
}

bool bc_feed(bc_capture_t *c, uint8_t n, bc_msg_t *out)
{
    /* Hysteresis: a grey value between BC_BAR_OFF and BC_BAR_ON keeps the
       current colour, so noise on an edge does not split an element. */
    bool bar = c->is_bar ? (n > BC_BAR_OFF) : (n >= BC_BAR_ON);

    if (!c->active)
    {
        /* The barcode strip is white except for the barcode, so the
           first bar seen while idle starts a capture. */
        if (bar)
        {
            c->active = true;
            c->is_bar = true;
            c->count = 0;
            c->overflow = false;
            c->run = 1;
            c->widest = 0;
        }
        return false;
    }

    if (bar == c->is_bar)
    { /* same colour: extend the run */
        if (c->run < BC_WIDTH_SAT)
        {
            c->run++;
        }
        if (!bar && c->run >= bc_quiet_samples(c))
        {
            /* long white after the last bar: barcode is over.
               The trailing white is not stored. */
            return bc_finish(c, out);
        }
        return false;
    }

    /* Colour changed: the previous element is complete. */
    if (c->run > c->widest)
    {
        c->widest = c->run;
    }
    bc_push(c, c->run);
    c->is_bar = bar;
    c->run = 1;

    if (BC_EXPECTED_ELEMS != 0u && c->count == BC_EXPECTED_ELEMS &&
        !c->overflow)
    {
        /* Last bar of the stop character finished: send now rather than
           waiting BC_QUIET_MS (the brief asks for immediate action). */
        return bc_finish(c, out);
    }
    return false;
}
