#include "subsystem1.h"
#include "telemetry_transport.h"
#define EVENT_CAPACITY 8u
/* Eight pending events cost 768 payload bytes. On a longer outage the FIFO
 * reports overflow; motor STOP and terrain interlocks use independent paths.
 * A lossless outage duration requires a measured event rate and link trace. */
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

static uint32_t control_boot,control_session;
#define CONTROL_QUEUE_DEPTH 4u
static int control_connected, control_have_status;
static unsigned control_head, control_count;
static control_command_t control_fifo[CONTROL_QUEUE_DEPTH];
/* The single Wi-Fi producer can post STOP while the transport mutex is busy.
 * Sequentially consistent 32-bit accesses let the consumer detect a torn copy
 * and retry next poll. The acknowledged version advances only after delivery. */
static uint32_t stop_version, stop_taken, stop_id, stop_issued, stop_epoch;
#define STOP_LOAD(word) __atomic_load_n(&(word), __ATOMIC_SEQ_CST)
#define STOP_STORE(word, value) __atomic_store_n(&(word), (value), __ATOMIC_SEQ_CST)
static uint8_t control_status[CONTROL_STATUS_SIZE];

void s1_control_boot(uint32_t boot)
{
    if (!lock()) return;
    __atomic_store_n(&control_boot, boot, __ATOMIC_RELEASE);
    tk_unl_mtx(transport_mutex);
}

int s1_control_link(uint32_t session, int connected)
{
    if (!lock()) return 0;
    if (control_session != session || !connected) {
        control_count = 0;
        STOP_STORE(stop_taken, STOP_LOAD(stop_version));
    }
    control_session = session;
    control_connected = connected;
    tk_unl_mtx(transport_mutex);
    return 1;
}

int s1_control_context(uint32_t *boot, uint32_t *session, int *connected)
{
    if (!lock()) return 0;
    *boot = __atomic_load_n(&control_boot, __ATOMIC_ACQUIRE);
    *session = control_session;
    *connected = control_connected;
    tk_unl_mtx(transport_mutex);
    return 1;
}

int s1_control_offer(const uint8_t *frame, unsigned length, uint32_t now_ms)
{
    control_command_t cmd;
    if (!control_decode(frame, length, &cmd) ||
        now_ms - cmd.issued > CONTROL_MAX_AGE_MS || !cmd.boot || !cmd.session)
        return 0;
    /* Link state and session changes run on this same Wi-Fi producer. */
    if (!control_connected || cmd.session != control_session ||
        cmd.boot != __atomic_load_n(&control_boot, __ATOMIC_ACQUIRE)) return 0;
    uint32_t version = STOP_LOAD(stop_version);
    if (cmd.op == CONTROL_STOP) {
        if (version != STOP_LOAD(stop_taken) && !control_newer(cmd.id, STOP_LOAD(stop_id)))
            return 1;
        STOP_STORE(stop_version, version + 1u);
        STOP_STORE(stop_id, cmd.id);
        STOP_STORE(stop_issued, cmd.issued);
        STOP_STORE(stop_epoch, cmd.epoch);
        STOP_STORE(stop_version, version + 2u);
        return 1;
    }
    if (!lock()) return 0;
    bool accepted = version == STOP_LOAD(stop_taken) && control_count < CONTROL_QUEUE_DEPTH;
    if (accepted) {
        control_fifo[(control_head + control_count) % CONTROL_QUEUE_DEPTH] = cmd;
        ++control_count;
    }
    tk_unl_mtx(transport_mutex);
    return accepted;
}

int s1_control_take(control_command_t *command, uint32_t now, uint32_t last_id,
                    uint32_t epoch, bool motion_fresh)
{
    if (!lock()) return 0;
    uint32_t version = STOP_LOAD(stop_version);
    bool found = version != STOP_LOAD(stop_taken) || control_count != 0u;
    if (version != STOP_LOAD(stop_taken)) {
        *command = (control_command_t){.op=CONTROL_STOP, .session=control_session,
            .boot=__atomic_load_n(&control_boot, __ATOMIC_ACQUIRE), .id=STOP_LOAD(stop_id),
            .issued=STOP_LOAD(stop_issued), .epoch=STOP_LOAD(stop_epoch)};
        if ((version & 1u) || version != STOP_LOAD(stop_version)) {
            found = false;
        } else {
            STOP_STORE(stop_taken, version);
            found = control_newer(command->id, last_id) &&
                now - command->issued <= CONTROL_MAX_AGE_MS &&
                (!motion_fresh || command->epoch == epoch);
            if (found) control_count = 0;
        }
    } else if (control_count != 0u) {
        *command = control_fifo[control_head];
        control_head = (control_head + 1u) % CONTROL_QUEUE_DEPTH;
        --control_count;
    }
    tk_unl_mtx(transport_mutex);
    return found;
}
int s1_control_status_offer(const uint8_t *frame) {
 if(!lock())return 0;
 for(unsigned i=0;i<CONTROL_STATUS_SIZE;++i)control_status[i]=frame[i];
 control_have_status=1;tk_unl_mtx(transport_mutex);return 1;
}
int s1_control_status_peek(uint8_t *frame) {
 if(!lock())return 0;
 int found=control_have_status;
 if(found)for(unsigned i=0;i<CONTROL_STATUS_SIZE;++i)frame[i]=control_status[i];
 tk_unl_mtx(transport_mutex);return found;
}
