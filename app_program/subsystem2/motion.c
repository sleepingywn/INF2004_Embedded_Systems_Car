#include "motion.h"
#include "motion_logic.h"
#include "drv_motor.h"
#include "drv_encoder.h"
#include <limits.h>
#include <string.h>
#if TK_SUPPORT_SMP
#include <tk/smp.h>
#endif

#define WAKE_BIT 1u
#define STOP_BIT 2u
#define SOURCE_BITS 0xf00u
#define SOURCE_BIT(s) (1u << (8u + (unsigned)(s)))

static ID mailbox_mutex;
static ID events;
static ID cyclic;
static UW stop_generation;
static UW fault_generation;
static UW handled_stop_generation;
static UW handled_fault_generation;

static UW generation_add(UW *value, UW increment)
{
#if TK_SUPPORT_SMP
    return atomic_add(value, increment);
#else
    UINT interrupt_state;
    DI(interrupt_state);
    __asm__ volatile("" ::: "memory");
    *value += increment;
    UW result = *value;
    __asm__ volatile("" ::: "memory");
    EI(interrupt_state);
    return result;
#endif
}
/* All shared application data below protected by mailbox_mutex on both cores.
 * Owner only polls; it stops rather than waiting behind another task. */
static struct
{
    motion_request_t request;
    motion_status_t status;
    line_msg_t line;
    bc_msg_t barcode;
    uint32_t line_ms;
    uint32_t line_epoch;
    uint32_t barcode_ms;
    uint32_t barcode_epoch;
    uint32_t rejected_requests;
    bool pending : 1;
    bool processing : 1;
    bool renewal : 1;
    bool have_line : 1;
    bool have_barcode : 1;
} shared;

/* These are exclusively owned by motion_task. */
static struct
{
    bool active_valid : 1;
    bool turning_from_line : 1;
    bool stop_seen : 1;
    bool encoder_ready : 1;
    bool motor_ready : 1;
} owner_flags;
static motion_status_t state;
static motion_request_t active;
static motion_pid_t pid[2];
static encoder_raw_t previous_counts;
static struct
{
    uint64_t distance_bits : 34; /* encoded +/-8,388,607,000 um */
    uint32_t fraction_bits : 21; /* encoded signed division remainder */
    uint64_t start_bits : 34;
    uint32_t target_bits : 30; /* encoded relative travel +/-524,287,000 um */
} wheels[2];
static uint32_t last_motion_ms[2];
static uint32_t last_update;
static uint32_t previous_encoder_ms;
static uint32_t manoeuvre_start;
static uint32_t reacquire_start;
static uint32_t last_barcode_ms;
static uint32_t last_invalid;
static uint32_t owner_rejections;
static uint32_t output_rest_until;
static struct
{
    uint16_t bits : 11;
} previous_duty[2];
static uint32_t reverse_until[2];
uint32_t motion_now_ms(void)
{
    /* RP2040 free-running 64-bit microsecond timer. Do not divide a raw 32-bit
     * low word: it wraps in 71 minutes and breaks millisecond subtraction. */
    UW high_before, high_after, low;
    do
    {
        high_before = in_w(TIMER_TIMERAWH);
        low = in_w(TIMER_TIMERAWL);
        high_after = in_w(TIMER_TIMERAWH);
    } while (high_before != high_after);
    return (uint32_t)((((uint64_t)high_before << 32) | low) / 1000u);
}

static bool fresh(uint32_t now, uint32_t then, uint32_t maximum)
{
    return (maximum != 0u) && ((uint32_t)(now - then) <= maximum);
}

static bool calibrated(void)
{
    return (MOTION_CALIBRATED != 0) && (MOTION_ENCODERS_VERIFIED != 0) &&
           ((MOTION_LEFT_MOTOR_SIGN == 1) || (MOTION_LEFT_MOTOR_SIGN == -1)) &&
           ((MOTION_RIGHT_MOTOR_SIGN == 1) || (MOTION_RIGHT_MOTOR_SIGN == -1)) &&
           ((MOTION_LEFT_ENCODER_SIGN == 1) || (MOTION_LEFT_ENCODER_SIGN == -1)) &&
           ((MOTION_RIGHT_ENCODER_SIGN == 1) || (MOTION_RIGHT_ENCODER_SIGN == -1)) &&
           (MOTION_LEFT_COUNTS_REV > 0u) && (MOTION_LEFT_COUNTS_REV <= 1000000u) &&
           (MOTION_RIGHT_COUNTS_REV > 0u) && (MOTION_RIGHT_COUNTS_REV <= 1000000u) &&
           (MOTION_LEFT_CIRCUMFERENCE_UM > 0u) && (MOTION_LEFT_CIRCUMFERENCE_UM <= 1000000u) &&
           (MOTION_RIGHT_CIRCUMFERENCE_UM > 0u) && (MOTION_RIGHT_CIRCUMFERENCE_UM <= 1000000u) &&
           (MOTION_TRACK_WIDTH_MM > 0u) && (MOTION_TRACK_WIDTH_MM <= 1000u) &&
           (MOTION_MAX_SPEED_MM_S > 0u) && (MOTION_MAX_SPEED_MM_S <= 2047u) &&
           (MOTION_CRUISE_MM_S > 0u) && (MOTION_CRUISE_MM_S <= MOTION_MAX_SPEED_MM_S) &&
           (MOTION_TURN_MM_S > 0u) && (MOTION_TURN_MM_S <= MOTION_MAX_SPEED_MM_S) &&
           (MOTION_DUTY_LIMIT > 0u) && (MOTION_DUTY_LIMIT <= 1000u) &&
           (MOTION_POSITION_TOLERANCE_MM > 0u) && (MOTION_POSITION_TOLERANCE_MM <= 100u) &&
           (MOTION_STALL_MS >= MOTION_PERIOD_MS) && (MOTION_STALL_MS <= 30000u) &&
           (MOTION_INPUT_MAX_AGE_MS >= 10u) && (MOTION_INPUT_MAX_AGE_MS < 16384u) &&
           (MOTION_MANOEUVRE_TIMEOUT_MS > 0u) && (MOTION_MANOEUVRE_TIMEOUT_MS <= 30000u) &&
           (MOTION_REVERSE_REST_MS >= MOTION_PERIOD_MS) && (MOTION_REVERSE_REST_MS <= 1000u) &&
           (MOTION_STALL_MS > MOTION_REVERSE_REST_MS + MOTION_PERIOD_MS) &&
           (MOTION_PID_KP_Q10 > 0u) && (MOTION_PID_KP_Q10 <= 65535u) &&
           (MOTION_PID_KI_Q10 <= 65535u) && (MOTION_PID_KD_Q10 <= 65535u) &&
           (MOTION_LINE_KP_Q10 > 0u) && (MOTION_LINE_KP_Q10 <= 65535u) &&
           (MOTION_STRAIGHT_KP_Q10 <= 65535u);
}

ER motion_get_status(motion_status_t *status)
{
    if (status == NULL)
    {
        return E_PAR;
    }
    if (mailbox_mutex <= 0)
    {
        return E_OBJ;
    }
    ER error = tk_loc_mtx(mailbox_mutex, TMO_POL);
    if (error < E_OK)
    {
        return error;
    }
    *status = shared.status;
    tk_unl_mtx(mailbox_mutex);
    return E_OK;
}

ER motion_stop(void)
{
    if (events <= 0)
    {
        return E_OBJ;
    }
    (void)generation_add(&stop_generation, 1);
    return tk_set_flg(events, STOP_BIT | WAKE_BIT);
}

ER motion_set_fault(motion_fault_source_t source, bool asserted)
{
    if ((unsigned)source > MOTION_HEALTH_TERRAIN)
    {
        return E_PAR;
    }
    if (events <= 0)
    {
        return E_OBJ;
    }
    if (asserted)
    {
        (void)generation_add(&fault_generation, 1);
    }
    ER error = asserted ? tk_set_flg(events, SOURCE_BIT(source) | WAKE_BIT)
                        : tk_clr_flg(events, ~SOURCE_BIT(source));
    if (!asserted && (error == E_OK))
    {
        error = tk_set_flg(events, WAKE_BIT);
    }
    return error;
}

static bool valid_request(const motion_request_t *request)
{
    if ((request->version != MOTION_SCHEMA) || (request->reserved != 0u) || (request->id == 0u) ||
        (request->ttl_ms < 2u) || (request->ttl_ms > 30000u) || (request->command > MOTION_RESET))
    {
        return false;
    }
    int32_t left = motion_request_left(request);
    int32_t right = motion_request_right(request);
    if (request->command == MOTION_RESET)
    {
        return (request->source == MOTION_LOCAL) && (motion_request_value(request) == 0) &&
               (left == 0) && (right == 0);
    }
    if (request->command == MOTION_BENCH)
    {
        return (request->source == MOTION_LOCAL) && (motion_request_value(request) == 0) &&
               (request->ttl_ms <= MOTION_BENCH_TIME_MAX_MS) && (left >= -MOTION_BENCH_DUTY_MAX) &&
               (left <= MOTION_BENCH_DUTY_MAX) && (right >= -MOTION_BENCH_DUTY_MAX) &&
               (right <= MOTION_BENCH_DUTY_MAX);
    }
    /* Network transport is not movement authority. Buddy1 can publish status
     * and assert faults; an agreed local controller admits mission requests. */
    if (!calibrated() || (request->source == MOTION_TERRAIN) || (request->source == MOTION_NETWORK))
    {
        return false;
    }
    int32_t maximum = MOTION_MAX_SPEED_MM_S;
    if ((left < -maximum) || (left > maximum) || (right < -maximum) || (right > maximum))
    {
        return false;
    }
    switch (request->command)
    {
        case MOTION_DISTANCE:
            return (motion_request_value(request) != 0) &&
                   (motion_request_value(request) >= -524287) && (left > 0) && (right == 0);
        case MOTION_TURN:
            return (motion_request_value(request) >= -180) &&
                   (motion_request_value(request) <= 180) && (motion_request_value(request) != 0) &&
                   (left > 0) && (right == 0);
        case MOTION_FOLLOW:
            return (motion_request_value(request) == 0) && (left > 0) && (left < maximum) &&
                   (right == 0);
        case MOTION_VELOCITY:
            return motion_request_value(request) == 0;
        default:
            return false;
    }
}

ER motion_request_init(motion_request_t *request, motion_command_t command, motion_source_t source,
                       uint32_t epoch, uint32_t id, uint32_t ttl_ms, int32_t value, int32_t left,
                       int32_t right)
{
    if ((request == NULL) || ((unsigned)command > MOTION_RESET) ||
        ((unsigned)source > MOTION_TERRAIN) || (id == 0u) || (ttl_ms < 2u) || (ttl_ms > 30000u) ||
        (value < -524287) || (value > 524287) ||
        ((command == MOTION_TURN) && ((value < -180) || (value > 180))) || (left < -2047) ||
        (left > 2047) || (right < -2047) || (right > 2047))
    {
        return E_PAR;
    }
    memset(request, 0, sizeof(*request));
    request->epoch = epoch;
    request->id = id;
    request->issued_ms = motion_now_ms();
    request->ttl_ms = (uint16_t)ttl_ms;
    request->version = MOTION_SCHEMA;
    request->source = source;
    request->command = command;
    if (command == MOTION_TURN)
    {
        request->value.angle_bits = (uint16_t)motion_bits_encode(value, 9u);
    }
    else
    {
        request->value.distance_bits = (uint32_t)motion_bits_encode(value, 20u);
    }
    request->left_bits = (uint16_t)motion_bits_encode(left, 12u);
    request->right_bits = (uint16_t)motion_bits_encode(right, 12u);
    return valid_request(request) ? E_OK : E_PAR;
}

ER motion_submit(const motion_request_t *request)
{
    if ((request == NULL) || !valid_request(request))
    {
        return E_PAR;
    }
    if (mailbox_mutex <= 0)
    {
        return E_OBJ;
    }
    ER error = tk_loc_mtx(mailbox_mutex, TMO_POL);
    if (error < E_OK)
    {
        return error;
    }
    const motion_status_t *status = &shared.status;
    bool id_new = (request->epoch != status->request_epoch) || (request->id > status->request_id);
    bool idle = (status->state == MOTION_IDLE) || (status->state == MOTION_ERROR);
    if (shared.pending || shared.processing || !idle || !id_new ||
        (request->epoch != status->epoch) ||
        !fresh(motion_now_ms(), request->issued_ms, request->ttl_ms - 1u))
    {
        ++shared.rejected_requests;
        tk_unl_mtx(mailbox_mutex);
        return E_OBJ;
    }
    shared.request = *request;
    shared.pending = true;
    shared.renewal = false;
    tk_unl_mtx(mailbox_mutex);
    return tk_set_flg(events, WAKE_BIT);
}

ER motion_renew(uint32_t epoch, uint32_t id, motion_source_t source, uint32_t issued_ms,
                uint16_t ttl_ms)
{
    if ((ttl_ms < 2u) || (ttl_ms > 30000u) || ((unsigned)source > MOTION_TERRAIN))
    {
        return E_PAR;
    }
    if (mailbox_mutex <= 0)
    {
        return E_OBJ;
    }
    ER error = tk_loc_mtx(mailbox_mutex, TMO_POL);
    if (error < E_OK)
    {
        return error;
    }
    if (shared.pending || shared.processing || (shared.status.result != MOTION_ACCEPTED) ||
        (epoch != shared.status.epoch) || (id != shared.status.request_id) ||
        (source != shared.status.source) ||
        ((shared.request.command != MOTION_FOLLOW) &&
         (shared.request.command != MOTION_VELOCITY)) ||
        !fresh(motion_now_ms(), issued_ms, ttl_ms - 1u) ||
        ((uint32_t)(issued_ms - shared.request.issued_ms) == 0u) ||
        ((uint32_t)(issued_ms - shared.request.issued_ms) >= 0x80000000u))
    {
        tk_unl_mtx(mailbox_mutex);
        return E_OBJ;
    }
    shared.request.issued_ms = issued_ms;
    shared.request.ttl_ms = ttl_ms;
    shared.pending = true;
    shared.renewal = true;
    tk_unl_mtx(mailbox_mutex);
    return tk_set_flg(events, WAKE_BIT);
}

ER motion_publish_line(const line_msg_t *line, uint32_t sampled_ms, uint32_t epoch)
{
    if ((line == NULL) || (line->err < -100) || (line->err > 100) || (line->state > LINE_FAULT) ||
        (line->t_ms != (sampled_ms & 0x7fffu)))
    {
        return E_PAR;
    }
    if (mailbox_mutex <= 0)
    {
        return E_OBJ;
    }
    ER error = tk_loc_mtx(mailbox_mutex, TMO_POL);
    if (error < E_OK)
    {
        return error;
    }
    if ((epoch != shared.status.epoch) ||
        (shared.have_line && (shared.line_epoch == epoch) &&
         (((uint32_t)(sampled_ms - shared.line_ms) >= 0x80000000u) ||
          (sampled_ms == shared.line_ms))))
    {
        tk_unl_mtx(mailbox_mutex);
        return E_OBJ;
    }
    shared.line = *line;
    shared.line_ms = sampled_ms;
    shared.line_epoch = epoch;
    shared.have_line = true;
    tk_unl_mtx(mailbox_mutex);
    return tk_set_flg(events, WAKE_BIT);
}

ER motion_publish_barcode(const bc_msg_t *barcode, uint32_t sampled_ms, uint32_t epoch)
{
    if ((barcode == NULL) || (barcode->hdr.count > BC_MAX_ELEMS))
    {
        return E_PAR;
    }
    if (mailbox_mutex <= 0)
    {
        return E_OBJ;
    }
    ER error = tk_loc_mtx(mailbox_mutex, TMO_POL);
    if (error < E_OK)
    {
        return error;
    }
    if (shared.have_barcode || (epoch != shared.status.epoch))
    {
        tk_unl_mtx(mailbox_mutex);
        return E_OBJ;
    }
    shared.barcode = *barcode;
    shared.barcode_ms = sampled_ms;
    shared.barcode_epoch = epoch;
    shared.have_barcode = true;
    tk_unl_mtx(mailbox_mutex);
    return tk_set_flg(events, WAKE_BIT);
}

static void reset_control(void)
{
    motion_pid_reset(&pid[0]);
    motion_pid_reset(&pid[1]);
    previous_duty[0].bits = previous_duty[1].bits = 0;
    reverse_until[0] = reverse_until[1] = 0;
}

static void end_request(unsigned result)
{
    if (owner_flags.motor_ready)
    {
        motor_hw_stop();
    }
    state.left_duty_bits = state.right_duty_bits = 0;
    if (owner_flags.active_valid)
    {
        output_rest_until =
            motion_now_ms() +
            ((state.state == MOTION_PULSING) ? MOTION_BENCH_TIME_MAX_MS : MOTION_REVERSE_REST_MS);
    }
    reset_control();
    if (owner_flags.active_valid)
    {
        state.result = result;
    }
    owner_flags.active_valid = false;
    owner_flags.turning_from_line = false;
    state.state = MOTION_IDLE;
}

static void fault_stop(unsigned fault)
{
    bool first = state.faults == 0u;
    end_request(MOTION_FAILED);
    state.faults |= fault;
    state.state = MOTION_ERROR;
    state.valid = 0;
    if (first)
    {
        ++state.epoch;
    }
}

static void begin_turn(int angle, uint32_t now)
{
    /* Positive angle = left/CCW. Each wheel travels track*pi*angle/360.
     * 355/113 approximates pi; calibration/slip dominates this rounding. */
    int64_t arc_um = (int64_t)MOTION_TRACK_WIDTH_MM * 1000 * angle * 355 / (360 * 113);
    wheels[0].start_bits = wheels[0].distance_bits;
    wheels[1].start_bits = wheels[1].distance_bits;
    wheels[0].target_bits = (uint32_t)motion_bits_encode(-arc_um, 30u);
    wheels[1].target_bits = (uint32_t)motion_bits_encode(arc_um, 30u);
    manoeuvre_start = now;
    motor_hw_stop();
    state.left_duty_bits = state.right_duty_bits = 0;
    output_rest_until = now + MOTION_REVERSE_REST_MS;
    last_motion_ms[0] = last_motion_ms[1] = now;
    state.state = MOTION_TURNING;
    reset_control();
}

static void admit(const motion_request_t *request, uint32_t now, UINT flags)
{
    if (owner_flags.active_valid)
    {
        ++owner_rejections;
        return;
    }
    state.request_id = request->id;
    state.request_epoch = request->epoch;
    state.source = request->source;
    state.result = MOTION_REJECTED;
    if ((request->epoch != state.epoch) || !valid_request(request) ||
        !fresh(now, request->issued_ms, request->ttl_ms - 1u))
    {
        return;
    }
    if (request->command == MOTION_RESET)
    {
        if ((flags & SOURCE_BITS) != 0u || !owner_flags.motor_ready)
        {
            return;
        }
        /* The generation counters preserve a stop/fault arriving while reset
         * clears its event bit. It will be handled on the next wake, not lost. */
        if ((generation_add(&stop_generation, 0) != handled_stop_generation) ||
            (generation_add(&fault_generation, 0) != handled_fault_generation))
        {
            return;
        }
        motor_hw_stop();
        state.left_duty_bits = state.right_duty_bits = 0;
        state.faults = 0;
        state.state = MOTION_IDLE;
        state.result = MOTION_COMPLETED;
        ++state.epoch;
        owner_flags.stop_seen = false;
        tk_clr_flg(events, ~STOP_BIT);
        reset_control();
        if (owner_flags.encoder_ready)
        {
            encoder_hw_snapshot(&previous_counts);
            previous_encoder_ms = motion_now_ms();
            last_invalid = previous_counts.invalid;
        }
        wheels[0].distance_bits = wheels[1].distance_bits = 0;
        wheels[0].fraction_bits = wheels[1].fraction_bits = 0;
        state.left_distance_bits = state.right_distance_bits = 0;
        state.valid = 0; /* next coherent encoder sample establishes validity */
        state.left_speed_bits = state.right_speed_bits = 0;
        return;
    }
    if ((state.faults != 0u) || ((flags & (STOP_BIT | SOURCE_BITS)) != 0u) ||
        !owner_flags.motor_ready || ((request->command != MOTION_BENCH) && !state.valid) ||
        ((output_rest_until != 0u) && (output_rest_until != now) &&
         ((uint32_t)(output_rest_until - now) < 0x80000000u)))
    {
        return;
    }
    active = *request;
    owner_flags.active_valid = true;
    state.result = MOTION_ACCEPTED;
    wheels[0].start_bits = wheels[0].distance_bits;
    wheels[1].start_bits = wheels[1].distance_bits;
    last_motion_ms[0] = last_motion_ms[1] = now;
    reset_control();
    switch (request->command)
    {
        case MOTION_BENCH:
            state.state = MOTION_PULSING;
            motor_hw_write((int16_t)motion_request_left(request),
                           (int16_t)motion_request_right(request));
            state.left_duty_bits = (uint16_t)motion_bits_encode(motion_request_left(request), 11u);
            state.right_duty_bits =
                (uint16_t)motion_bits_encode(motion_request_right(request), 11u);
            break;
        case MOTION_DISTANCE:
            state.state = MOTION_TRAVELLING;
            wheels[0].target_bits = wheels[1].target_bits =
                (uint32_t)motion_bits_encode((int64_t)motion_request_value(request) * 1000, 30u);
            break;
        case MOTION_TURN:
            begin_turn(motion_request_value(request), now);
            break;
        case MOTION_FOLLOW:
            state.state = MOTION_TRACING;
            last_barcode_ms = now;
            break;
        default:
            state.state = MOTION_DRIVING;
            break;
    }
}

static bool update_odometry(uint32_t now)
{
    encoder_raw_t raw;
    if (!owner_flags.encoder_ready)
    {
        state.valid = 0;
        return false;
    }
    encoder_hw_snapshot(&raw);
    uint32_t acquired_ms = motion_now_ms();
    uint32_t sample_dt = acquired_ms - previous_encoder_ms;
    previous_encoder_ms = acquired_ms;
    state.time_ms = acquired_ms;
    state.left_count = raw.left;
    state.right_count = raw.right;
    state.invalid_edges = raw.invalid;
    if (!calibrated())
    {
        previous_counts = raw;
        state.valid = 0;
        return false;
    }
    if ((sample_dt == 0u) || (sample_dt > 100u))
    {
        fault_stop(MOTION_FAULT_TIMEOUT);
        return false;
    }
    if (raw.invalid != last_invalid)
    {
        fault_stop(MOTION_FAULT_ENCODER);
        return false;
    }
    int32_t deltas[2] = {motion_count_delta(raw.left, previous_counts.left),
                         motion_count_delta(raw.right, previous_counts.right)};
    previous_counts = raw;
    const int signs[2] = {MOTION_LEFT_ENCODER_SIGN, MOTION_RIGHT_ENCODER_SIGN};
    const uint32_t circumferences[2] = {MOTION_LEFT_CIRCUMFERENCE_UM,
                                        MOTION_RIGHT_CIRCUMFERENCE_UM};
    const uint32_t revolutions[2] = {MOTION_LEFT_COUNTS_REV, MOTION_RIGHT_COUNTS_REV};
    int32_t speeds[2];
    for (unsigned i = 0; i < 2u; ++i)
    {
        if (deltas[i] != 0)
        {
            last_motion_ms[i] = now;
        }
        int64_t numerator = (int64_t)deltas[i] * signs[i] * circumferences[i] +
                            motion_bits_decode(wheels[i].fraction_bits, 21u);
        int64_t travel = numerator / revolutions[i];
        wheels[i].fraction_bits = (uint32_t)motion_bits_encode(numerator % revolutions[i], 21u);
        int64_t next_distance = motion_bits_decode(wheels[i].distance_bits, 34u) + travel;
        int64_t velocity = travel / sample_dt; /* um/ms == mm/s */
        if ((velocity < -2047) || (velocity > 2047) || (next_distance < -INT64_C(8388607000)) ||
            (next_distance > INT64_C(8388607000)))
        {
            fault_stop(MOTION_FAULT_RANGE);
            return false;
        }
        wheels[i].distance_bits = motion_bits_encode(next_distance, 34u);
        speeds[i] = (int32_t)velocity;
    }
    state.left_speed_bits = (uint16_t)motion_bits_encode(speeds[0], 12u);
    state.right_speed_bits = (uint16_t)motion_bits_encode(speeds[1], 12u);
    state.left_distance_bits =
        (uint32_t)motion_bits_encode(motion_bits_decode(wheels[0].distance_bits, 34u) / 1000, 24u);
    state.right_distance_bits =
        (uint32_t)motion_bits_encode(motion_bits_decode(wheels[1].distance_bits, 34u) / 1000, 24u);
    state.valid = (state.faults == 0u);
    return state.valid != 0u;
}

static int16_t limit_speed(int64_t speed)
{
    if (speed > (int32_t)MOTION_MAX_SPEED_MM_S)
    {
        speed = MOTION_MAX_SPEED_MM_S;
    }
    if (speed < -(int32_t)MOTION_MAX_SPEED_MM_S)
    {
        speed = -(int32_t)MOTION_MAX_SPEED_MM_S;
    }
    return (int16_t)speed;
}

static void apply_velocity(int16_t left, int16_t right, uint32_t now, uint16_t dt)
{
    const int16_t targets[2] = {left, right};
    const int16_t measured[2] = {(int16_t)motion_left_speed(&state),
                                 (int16_t)motion_right_speed(&state)};
    const int signs[2] = {MOTION_LEFT_MOTOR_SIGN, MOTION_RIGHT_MOTOR_SIGN};
    int16_t duty[2];
    for (unsigned i = 0; i < 2u; ++i)
    {
        if (targets[i] == 0)
        {
            last_motion_ms[i] = now;
            motion_pid_reset(&pid[i]);
            duty[i] = 0;
        }
        else
        {
            if ((uint32_t)(now - last_motion_ms[i]) > MOTION_STALL_MS)
            {
                fault_stop(MOTION_FAULT_STALL);
                return;
            }
            duty[i] = motion_pid_step(&pid[i], targets[i], measured[i], dt, MOTION_PID_KP_Q10,
                                      MOTION_PID_KI_Q10, MOTION_PID_KD_Q10, MOTION_DUTY_LIMIT);
            /* Do not reverse electrically just to slow down: brake until the
             * commanded travel direction requires a reversal. */
            if (((targets[i] > 0) && (duty[i] < 0)) || ((targets[i] < 0) && (duty[i] > 0)))
            {
                duty[i] = 0;
            }
            duty[i] = (int16_t)(duty[i] * signs[i]);
        }
        if (((motion_bits_decode(previous_duty[i].bits, 11u) > 0) && (duty[i] < 0)) ||
            ((motion_bits_decode(previous_duty[i].bits, 11u) < 0) && (duty[i] > 0)))
        {
            reverse_until[i] = now + MOTION_REVERSE_REST_MS;
            motion_pid_reset(&pid[i]);
        }
        if ((reverse_until[i] != 0u) && ((uint32_t)(reverse_until[i] - now) < 0x80000000u) &&
            (reverse_until[i] != now))
        {
            duty[i] = 0;
        }
        else
        {
            reverse_until[i] = 0;
        }
        previous_duty[i].bits = (uint16_t)motion_bits_encode(duty[i], 11u);
    }
    motor_hw_write(duty[0], duty[1]);
    state.left_duty_bits = (uint16_t)motion_bits_encode(duty[0], 11u);
    state.right_duty_bits = (uint16_t)motion_bits_encode(duty[1], 11u);
}

static void control_step(uint32_t now, uint16_t dt, const line_msg_t *line, uint32_t line_ms,
                         uint32_t line_epoch, bool have_line)
{
    if (!owner_flags.active_valid || (state.state == MOTION_PULSING))
    {
        return;
    }
    if ((output_rest_until != 0u) && (output_rest_until != now) &&
        ((uint32_t)(output_rest_until - now) < 0x80000000u))
    {
        motor_hw_stop();
        state.left_duty_bits = state.right_duty_bits = 0;
        return;
    }
    output_rest_until = 0;
    int16_t left = (int16_t)motion_request_left(&active);
    int16_t right = (int16_t)motion_request_right(&active);
    if ((state.state == MOTION_TRAVELLING) || (state.state == MOTION_TURNING))
    {
        bool done[2];
        int16_t speeds[2];
        for (unsigned i = 0; i < 2u; ++i)
        {
            int64_t progress = motion_bits_decode(wheels[i].distance_bits, 34u) -
                               motion_bits_decode(wheels[i].start_bits, 34u);
            int64_t remaining = motion_bits_decode(wheels[i].target_bits, 30u) - progress;
            int direction = (motion_bits_decode(wheels[i].target_bits, 30u) < 0) ? -1 : 1;
            done[i] = direction * remaining <= (int64_t)MOTION_POSITION_TOLERANCE_MM * 1000;
            int32_t speed =
                owner_flags.turning_from_line ? MOTION_TURN_MM_S : motion_request_left(&active);
            speeds[i] = done[i] ? 0 : (int16_t)(direction * speed);
        }
        if (done[0] && done[1])
        {
            if (owner_flags.turning_from_line)
            {
                motor_hw_stop();
                state.left_duty_bits = state.right_duty_bits = 0;
                reset_control();
                state.state = MOTION_REACQUIRE;
                reacquire_start = now;
            }
            else
            {
                end_request(MOTION_COMPLETED);
            }
            return;
        }
        left = speeds[0];
        right = speeds[1];
        if ((state.state == MOTION_TRAVELLING) && !done[0] && !done[1])
        {
            int64_t mismatch = ((motion_bits_decode(wheels[0].distance_bits, 34u) -
                                 motion_bits_decode(wheels[0].start_bits, 34u)) -
                                (motion_bits_decode(wheels[1].distance_bits, 34u) -
                                 motion_bits_decode(wheels[1].start_bits, 34u))) /
                               1000;
            int64_t correction = mismatch * MOTION_STRAIGHT_KP_Q10 / 1024;
            /* Bound correction to half speed; never reverse a straight wheel. */
            int32_t bound = motion_request_left(&active) / 2;
            if (correction > bound)
            {
                correction = bound;
            }
            if (correction < -bound)
            {
                correction = -bound;
            }
            left = limit_speed(left - correction);
            right = limit_speed(right + correction);
        }
    }
    else if ((state.state == MOTION_TRACING) || (state.state == MOTION_REACQUIRE))
    {
        if (!have_line || (line_epoch != state.epoch) ||
            !fresh(now, line_ms, MOTION_INPUT_MAX_AGE_MS) || line->fault ||
            (line->state == LINE_FAULT) || (line->state == LINE_CALIBRATING))
        {
            fault_stop(MOTION_FAULT_IR);
            return;
        }
        if (state.state == MOTION_REACQUIRE)
        {
            if (((uint32_t)(line_ms - reacquire_start) > 0u) &&
                ((uint32_t)(line_ms - reacquire_start) < 0x80000000u) &&
                ((line->state == LINE_ON) || (line->state == LINE_EDGE_ONLY)))
            {
                state.state = MOTION_TRACING;
                owner_flags.turning_from_line = false;
                last_motion_ms[0] = last_motion_ms[1] = now;
            }
            else if (now - reacquire_start > MOTION_MANOEUVRE_TIMEOUT_MS)
            {
                fault_stop(MOTION_FAULT_TIMEOUT);
            }
            return; /* remain stopped until a NEW valid line sample */
        }
        if ((line->state == LINE_LOST) || (line->state == LINE_LOST_HOLD))
        {
            fault_stop(MOTION_FAULT_IR);
            return;
        }
        /* Buddy3 positive error = line to LEFT: reduce left, increase right.
         * Keep mean speed fixed while bc_active; no acceleration requests here. */
        if (!motion_line_targets((int8_t)line->err, (uint16_t)motion_request_left(&active),
                                 MOTION_MAX_SPEED_MM_S, MOTION_LINE_KP_Q10, &left, &right))
        {
            fault_stop(MOTION_FAULT_RANGE);
            return;
        }
    }
    else if ((state.state == MOTION_DRIVING) && (left == right) && (left != 0))
    {
        int64_t mismatch = ((motion_bits_decode(wheels[0].distance_bits, 34u) -
                             motion_bits_decode(wheels[0].start_bits, 34u)) -
                            (motion_bits_decode(wheels[1].distance_bits, 34u) -
                             motion_bits_decode(wheels[1].start_bits, 34u))) /
                           1000;
        int64_t correction = mismatch * MOTION_STRAIGHT_KP_Q10 / 1024;
        int32_t bound = ((left < 0) ? -left : left) / 2;
        if (correction > bound)
        {
            correction = bound;
        }
        if (correction < -bound)
        {
            correction = -bound;
        }
        left = limit_speed(left - correction);
        right = limit_speed(right + correction);
    }
    apply_velocity(left, right, now, dt);
}

static void release_motion(void *unused)
{
    (void)unused;
    tk_set_flg(events, WAKE_BIT);
}

ER motion_init(void)
{
    T_CMTX mutex = {0};
    T_CFLG flag = {0};
    T_CCYC release = {0};
    mutex.mtxatr = TA_INHERIT;
    mailbox_mutex = tk_cre_mtx(&mutex);
    if (mailbox_mutex < E_OK)
    {
        return mailbox_mutex;
    }
    events = tk_cre_flg(&flag);
    if (events < E_OK)
    {
        return events;
    }
    release.cycatr = TA_HLNG;
    release.cychdr = (FP)release_motion;
    release.cyctim = MOTION_PERIOD_MS;
    release.cycphs = MOTION_PERIOD_MS;
    cyclic = tk_cre_cyc(&release);
    state.epoch = 1;
    state.calibrated = calibrated();
    shared.status = state;
    return (cyclic < E_OK) ? cyclic : E_OK;
}

ER motion_start(void)
{
    ID task = app_start_task((FP)motion_task, MOTION_PRIORITY, TP_PRC2, "motion_task");
    if (task < E_OK)
    {
        return task;
    }
    return tk_sta_cyc(cyclic);
}

void motion_task(INT stacd, void *exinf)
{
    (void)stacd;
    (void)exinf;
    owner_flags.motor_ready = (motor_hw_init() == E_OK);
    if (!owner_flags.motor_ready)
    {
        fault_stop(MOTION_FAULT_HARDWARE);
    }
    if (MOTION_ENCODERS_VERIFIED)
    {
        owner_flags.encoder_ready = (encoder_hw_init() == E_OK);
        if (!owner_flags.encoder_ready)
        {
            fault_stop(MOTION_FAULT_ENCODER);
        }
    }
    last_update = motion_now_ms();
    previous_encoder_ms = last_update;
    for (;;)
    {
        UINT wake;
        T_RFLG flags;
        ER waited = tk_wai_flg(events, WAKE_BIT, TWF_ORW | TWF_BITCLR, &wake, MOTION_PERIOD_MS);
        if ((waited < E_OK) && (waited != E_TMOUT))
        {
            fault_stop(MOTION_FAULT_HANDOFF);
        }
        if (tk_ref_flg(events, &flags) < E_OK)
        {
            fault_stop(MOTION_FAULT_HANDOFF);
            continue;
        }
        uint32_t now = motion_now_ms();
        UW new_fault_generation = generation_add(&fault_generation, 0);
        UW new_stop_generation = generation_add(&stop_generation, 0);
        if (((flags.flgptn & SOURCE_BITS) != 0u) ||
            (new_fault_generation != handled_fault_generation))
        {
            fault_stop(MOTION_FAULT_EXTERNAL);
            handled_fault_generation = new_fault_generation;
        }
        if ((((flags.flgptn & STOP_BIT) != 0u) && !owner_flags.stop_seen) ||
            (new_stop_generation != handled_stop_generation))
        {
            end_request(MOTION_CANCELLED);
            ++state.epoch;
            owner_flags.stop_seen = true;
            handled_stop_generation = new_stop_generation;
            if (state.faults != 0u)
            {
                state.state = MOTION_ERROR;
            }
        }
        if (owner_flags.active_valid && ((uint32_t)(now - active.issued_ms) >= active.ttl_ms))
        {
            if (state.state == MOTION_PULSING)
            {
                end_request(MOTION_COMPLETED);
            }
            else if ((state.state == MOTION_TRAVELLING) || (state.state == MOTION_TURNING) ||
                     (state.state == MOTION_REACQUIRE))
            {
                fault_stop(MOTION_FAULT_TIMEOUT);
            }
            else
            {
                end_request(MOTION_EXPIRED);
                ++state.epoch;
            }
        }
        if (owner_flags.turning_from_line && (now - manoeuvre_start > MOTION_MANOEUVRE_TIMEOUT_MS))
        {
            fault_stop(MOTION_FAULT_TIMEOUT);
        }
        if (tk_loc_mtx(mailbox_mutex, TMO_POL) < E_OK)
        {
            fault_stop(MOTION_FAULT_HANDOFF);
            continue;
        }
        motion_request_t request = shared.request;
        bool pending = shared.pending;
        bool renewal = shared.renewal;
        if (pending)
        {
            shared.processing = true;
        }
        shared.pending = false;
        line_msg_t line = shared.line;
        uint32_t line_ms = shared.line_ms;
        uint32_t line_epoch = shared.line_epoch;
        bool have_line = shared.have_line;
        bc_msg_t barcode = shared.barcode;
        bool have_barcode = shared.have_barcode;
        uint32_t barcode_ms = shared.barcode_ms;
        uint32_t barcode_epoch = shared.barcode_epoch;
        shared.have_barcode = false;
        tk_unl_mtx(mailbox_mutex);

        uint32_t dt = now - last_update;
        if (dt >= MOTION_PERIOD_MS)
        {
            if (dt > 100u)
            {
                fault_stop(MOTION_FAULT_TIMEOUT);
            }
            else
            {
                (void)update_odometry(now);
            }
            last_update = now;
        }
        if (pending && renewal)
        {
            if (owner_flags.active_valid && (active.epoch == request.epoch) &&
                (active.id == request.id) && (active.source == request.source) &&
                fresh(now, request.issued_ms, request.ttl_ms - 1u))
            {
                active.issued_ms = request.issued_ms;
                active.ttl_ms = request.ttl_ms;
            }
        }
        else if (pending)
        {
            admit(&request, now, flags.flgptn);
            if (state.result == MOTION_REJECTED)
            {
                ++owner_rejections;
            }
        }
        /* Do not admit a barcode from a failed/late line producer merely
         * because its standalone capture has a plausible pattern. */
        if (have_barcode)
        {
            char decoded = motion_barcode_decode(&barcode);
            bool fresh_capture = fresh(now, barcode_ms, MOTION_INPUT_MAX_AGE_MS) &&
                                 (barcode_epoch == state.epoch) &&
                                 ((uint32_t)(barcode_ms - last_barcode_ms) > 0u) &&
                                 ((uint32_t)(barcode_ms - last_barcode_ms) < 0x80000000u);
            if (owner_flags.active_valid && (state.state == MOTION_TRACING) && fresh_capture &&
                decoded && have_line && (line_epoch == state.epoch) &&
                fresh(now, line_ms, MOTION_INPUT_MAX_AGE_MS) && !line.fault &&
                (line.state != LINE_FAULT) && (line.state != LINE_LOST) &&
                (line.state != LINE_CALIBRATING))
            {
                last_barcode_ms = barcode_ms;
                state.barcode = (unsigned)(decoded - 'A' + 1);
                if (decoded != 'C')
                {
                    owner_flags.turning_from_line = true;
                    begin_turn((decoded == 'A') ? 90 : (decoded == 'B') ? -90 : 180, now);
                }
            }
            else
            {
                ++state.rejected_barcodes;
            }
        }
        if ((dt >= MOTION_PERIOD_MS) && (dt <= 100u) && (state.faults == 0u))
        {
            control_step(now, (uint16_t)dt, &line, line_ms, line_epoch, have_line);
        }
        state.updated_ms = motion_now_ms();
        ++state.sequence;
        if (tk_loc_mtx(mailbox_mutex, TMO_POL) == E_OK)
        {
            /* Preserve rejections that raced with this owner iteration. */
            state.rejected_requests = shared.rejected_requests + owner_rejections;
            shared.status = state;
            shared.processing = false;
            tk_unl_mtx(mailbox_mutex);
        }
        else
        {
            fault_stop(MOTION_FAULT_HANDOFF);
        }
    }
}
