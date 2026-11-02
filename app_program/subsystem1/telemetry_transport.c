#include "subsystem1.h"
#include "telemetry_transport.h"
#define EVENT_CAPACITY 8u
static ID transport_mutex;
static uint8_t latest[S1_FRAME_SIZE], events[EVENT_CAPACITY][S1_FRAME_SIZE];
static uint32_t latest_ticket, event_ticket, dropped;
static unsigned head, count;
static int pending;
static void copy_frame(uint8_t *to, const uint8_t *from)
{
    unsigned i;
    for (i = 0; i < S1_FRAME_SIZE; ++i) to[i] = from[i];
}
static int lock(void)
{
    return transport_mutex > 0 && tk_loc_mtx(transport_mutex, TMO_POL) == E_OK;
}
int s1_transport_init(void)
{
    T_CMTX cm = {0};
    cm.mtxatr = TA_INHERIT;
    transport_mutex = tk_cre_mtx(&cm);
    return transport_mutex < E_OK ? (int)transport_mutex : E_OK;
}
int s1_transport_offer(const uint8_t *frame, int event)
{
    if (!frame || !lock()) return 0;
    if (event) {
        if (count == EVENT_CAPACITY) {
            if (dropped != UINT32_MAX) ++dropped;
            tk_unl_mtx(transport_mutex);
            return 0;
        }
        copy_frame(events[(head + count) % EVENT_CAPACITY], frame);
        ++count;
    } else {
        copy_frame(latest, frame);
        ++latest_ticket;
        pending = 1;
    }
    tk_unl_mtx(transport_mutex);
    return 1;
}
int s1_transport_peek(uint8_t *frame, int *event, uint32_t *ticket)
{
    int found = 0;
    if (!frame || !event || !ticket || !lock()) return 0;
    if (count) {
        copy_frame(frame, events[head]);
        *event = 1; *ticket = event_ticket; found = 1;
    } else if (pending) {
        copy_frame(frame, latest);
        *event = 0; *ticket = latest_ticket; found = 1;
    }
    tk_unl_mtx(transport_mutex);
    return found;
}
void s1_transport_ack(int event, uint32_t ticket)
{
    if (!lock()) return; /* resend is allowed; receivers deduplicate sequence */
    if (event && count && ticket == event_ticket) {
        head = (head + 1u) % EVENT_CAPACITY;
        --count; ++event_ticket;
    } else if (!event && pending && ticket == latest_ticket) {
        pending = 0;
    }
    tk_unl_mtx(transport_mutex);
}
uint32_t s1_transport_dropped(void)
{
    uint32_t value;
    if (!lock()) return UINT32_MAX; /* explicitly unavailable */
    value = dropped;
    tk_unl_mtx(transport_mutex);
    return value;
}
