#include "ir_runtime.h"
#include "../subsystem2/motion_ir_bridge.h"
#if MOTION_LINE_CIRCUIT
#include "../../config/line_boot_calibration.h"
#endif
#define memset __builtin_memset /* not <string.h>: its size_t clashes with tk/syslib.h */
#define memcpy __builtin_memcpy

#define IR_COMMAND_EVENT 2u
#define IR_TASK_PRIORITY 16
#define IR_WATCHDOG_MS 20u /* liveness watchdog; not the 1 kHz pacing source */
#define IR_MAX_QUEUE_AGE_MS (IR_ACQ_RING_SIZE - 1u)

static ID runtime_mutex;

/* INTEGRATION TEST BUILD: a single TMO_POL attempt failed whenever another
 * core held the lock for a few microseconds, which raised false HANDOFF/IR
 * faults on the car (2026-10-31). Retry briefly, then wait at most 2 ms. */
static ER lock_runtime(void)
{
    for (unsigned i = 0; i < 50u; ++i)
    {
        ER e = tk_loc_mtx(runtime_mutex, TMO_POL);
        if (e != E_TMOUT)
        {
            return e;
        }
    }
    return tk_loc_mtx(runtime_mutex, 2);
}
static ID runtime_event;
static ir_runtime_status_t published;
static struct
{
    uint8_t command : 2;
    bool processing : 1;
} pending;
/* Everything below is private to ir_task. Other cores use the mutex copy. */
static ir_runtime_status_t owner;
static ir_cal_acc_t accumulator;
static motion_ir_bridge_t bridge;
static struct
{
    bool epoch_known : 1;
    bool health_asserted : 1;
} runtime_flags;
static uint32_t epoch_floor_ms;

static void set_health(bool healthy)
{
    /* A new BEGIN from another core must not race a successful sample into
     * clearing its source fault. The command API asserts under this mutex. */
    if (healthy)
    {
        if (lock_runtime() < E_OK)
        {
            return;
        }
        if ((pending.command != 0u) || pending.processing)
        {
            tk_unl_mtx(runtime_mutex);
            return;
        }
    }
    owner.healthy = healthy;
    if (runtime_flags.health_asserted != !healthy)
    {
        (void)motion_set_fault(MOTION_HEALTH_IR, !healthy);
        runtime_flags.health_asserted = !healthy;
    }
    if (healthy)
    {
        tk_unl_mtx(runtime_mutex);
    }
}

static void runtime_fault(void)
{
    ir_acquisition_stop();
    owner.acquisition.running = false;
    owner.state = IR_RUNTIME_FAULT;
    memset(owner.normalized, 0, sizeof(owner.normalized));
    motion_ir_bridge_init(&bridge);
    set_health(false);
}

static void timing_fault(ir_fault_reason_t reason, uint32_t detail_us)
{
    /* Freeze acquisition before recording a task failure. An IRQ may already
     * have latched the original cause since the last published snapshot. */
    ir_acquisition_stop();
    ir_raw_frame_t unused;
    ir_acq_status_t live;
    (void)ir_acquisition_pop(&unused, &live);
    if (owner.acquisition.first_fault_reason == IR_REASON_NONE) {
        live.faults |= owner.acquisition.faults;
        owner.acquisition = live;
    }
    if (owner.acquisition.first_fault_reason == IR_REASON_NONE) {
        owner.acquisition.first_fault_us = in_w(TIMER_TIMERAWL);
        owner.acquisition.first_fault_reason = reason;
        owner.acquisition.first_fault_detail = detail_us;
        owner.acquisition.first_fault_samples = owner.acquisition.samples;
    }
    owner.acquisition.faults |= IR_ACQ_TIMING;
    runtime_fault();
}

static void apply_command(unsigned command)
{
    if (command == IR_RUNTIME_BEGIN)
    {
        set_health(false);
        motion_ir_bridge_init(&bridge);
        ir_cal_begin(&accumulator);
        memset(owner.calibration, 0, sizeof(owner.calibration));
        memset(owner.normalized, 0, sizeof(owner.normalized));
        owner.calibration_samples = 0;
        owner.failed_channels = 7u;
        owner.acquisition = (ir_acq_status_t){0};
        owner.state = IR_RUNTIME_CALIBRATING;
        runtime_flags.epoch_known = false;
        if (ir_acquisition_start(runtime_event) < E_OK)
        {
            owner.acquisition.faults = IR_ACQ_HARDWARE;
            runtime_fault();
        }
    }
    else if ((command == IR_RUNTIME_FINISH) && (owner.state == IR_RUNTIME_CALIBRATING))
    {
        /* Only samples already processed by this task belong to calibration.
         * Discard queued frames so a pre-FINISH barcode can never be replayed. */
        ir_acquisition_discard();
        owner.failed_channels = ir_cal_finish(&accumulator, owner.calibration);
        if (owner.failed_channels != 0u)
        {
            runtime_fault();
        }
        else
        {
            owner.state = IR_RUNTIME_READY;
            motion_ir_bridge_init(&bridge);
            runtime_flags.epoch_known = false;
            /* Clear the live source fault only after a fresh successful handoff. */
        }
    }
}

static void process_frame(const ir_raw_frame_t *frame)
{
    uint32_t now = motion_now_ms();
    if ((uint32_t)(now - frame->sampled_ms) > IR_MAX_QUEUE_AGE_MS)
    {
        timing_fault(IR_REASON_QUEUE_AGE, (uint32_t)(now - frame->sampled_ms) * 1000u);
        return;
    }
    owner.raw = *frame;
    uint16_t raw[IR_NUM_CH] = {frame->barcode, frame->line_a, frame->line_b};
    if (owner.state == IR_RUNTIME_CALIBRATING)
    {
        ir_cal_add(&accumulator, raw);
        if (owner.calibration_samples != UINT32_MAX)
        {
            ++owner.calibration_samples;
        }
    }
    if (owner.state != IR_RUNTIME_READY)
    {
        return;
    }
    motion_status_t motion;
    ER status_error = motion_get_ir_status(&motion);
    if (status_error < E_OK)
    {
        ++owner.handoff_errors;
        owner.last_handoff_error = status_error;
        owner.last_handoff_ms = frame->sampled_ms;
        owner.last_handoff_stage = 1u;
        runtime_fault();
        return;
    }
    if (!runtime_flags.epoch_known || (motion.epoch != owner.epoch))
    {
        owner.epoch = motion.epoch;
        epoch_floor_ms = motion_now_ms();
        runtime_flags.epoch_known = true;
        motion_ir_bridge_init(&bridge);
        ir_acquisition_discard();
        return; /* reject samples acquired before observing the new epoch */
    }
    uint32_t since_epoch = frame->sampled_ms - epoch_floor_ms;
    if ((since_epoch == 0u) || (since_epoch >= 0x80000000u))
    {
        return;
    }
    uint8_t normalized[IR_NUM_CH];
    for (unsigned ch = 0; ch < IR_NUM_CH; ++ch)
    {
        normalized[ch] = ir_normalise(&owner.calibration[ch], raw[ch]);
        owner.normalized[ch].value = normalized[ch];
    }
    ER error = motion_ir_bridge_sample(&bridge, normalized, true, frame->sampled_ms, owner.epoch);
    if (error == MOTION_IR_SUPERSEDED)
    {
        /* STOP/reset raced the earlier status read. Reject old data, including
         * partial/completed barcodes, but keep acquisition running. Observe a
         * new coherent epoch and timestamp fence before admitting any frame. */
        ++owner.epoch_resyncs;
        runtime_flags.epoch_known = false;
        motion_ir_bridge_init(&bridge);
        ir_acquisition_discard();
        return; /* do not clear a real health fault or replay this sample */
    }
    if (error < E_OK)
    {
        ++owner.handoff_errors;
        owner.last_handoff_error = error;
        owner.last_handoff_ms = frame->sampled_ms;
        owner.last_handoff_stage = 2u;
        /* The bridge can assert the source itself; keep our mirror consistent. */
        runtime_flags.health_asserted = false;
        runtime_fault();
    }
    else
    {
        set_health(true);
    }
}

static void publish_status(void)
{
    owner.updated_ms = motion_now_ms();
    if (lock_runtime() == E_OK)
    {
        published = owner;
        pending.processing = false;
        published.command_pending = pending.command != 0u;
        tk_unl_mtx(runtime_mutex);
    }
}

static void ir_task(INT stacd, void *exinf)
{
    (void)stacd;
    (void)exinf;
    set_health(false);
    /* Do not fill the 63 ms acquisition ring during radio initialization.
     * Motion's bounded startup gate releases independently of AP connection. */
    uint32_t startup_ms = motion_now_ms();
    motion_status_t startup_motion;
    while (motion_get_status(&startup_motion) != E_OK || !startup_motion.ready) {
        if ((uint32_t)(motion_now_ms() - startup_ms) > 6000u) {
            timing_fault(IR_REASON_STARTUP, (uint32_t)(motion_now_ms() - startup_ms) * 1000u);
            publish_status();
            break;
        }
        tk_dly_tsk(20);
    }
    if (owner.state != IR_RUNTIME_FAULT && ir_acquisition_start(runtime_event) < E_OK)
    {
        owner.acquisition.faults = IR_ACQ_HARDWARE;
        runtime_fault();
    }
    for (;;)
    {
        unsigned command = 0;
        if (lock_runtime() == E_OK)
        {
            command = pending.command;
            pending.command = 0;
            pending.processing = pending.processing || (command != 0u);
            tk_unl_mtx(runtime_mutex);
        }
        apply_command(command);
        ir_raw_frame_t frame;
        ir_acq_status_t status;
        /* Bounded work even if interrupts continually add frames. */
        for (unsigned count = 0; count < IR_ACQ_RING_SIZE; ++count)
        {
            bool available = ir_acquisition_pop(&frame, &status);
            status.faults |= owner.acquisition.faults;
            /* Task-level failures must survive subsequent acquisition snapshots. */
            if (owner.acquisition.first_fault_reason != IR_REASON_NONE) {
                status.first_fault_reason = owner.acquisition.first_fault_reason;
                status.first_fault_us = owner.acquisition.first_fault_us;
                status.first_fault_detail = owner.acquisition.first_fault_detail;
                status.first_fault_samples = owner.acquisition.first_fault_samples;
            }
            owner.acquisition = status;
            if (status.faults != 0u)
            {
                runtime_fault();
                break;
            }
            if (!available)
            {
                break;
            }
            process_frame(&frame);
            if (owner.state == IR_RUNTIME_FAULT)
            {
                break;
            }
        }
        publish_status();
        UINT flags;
        uint32_t wait_started_ms = motion_now_ms();
        ER error = tk_wai_flg(runtime_event, IR_ACQ_EVENT | IR_COMMAND_EVENT, TWF_ORW | TWF_BITCLR,
                              &flags, IR_WATCHDOG_MS);
        if ((error < E_OK) && (owner.state != IR_RUNTIME_FAULT))
        {
            timing_fault(IR_REASON_WATCHDOG, (uint32_t)(motion_now_ms() - wait_started_ms) * 1000u);
            publish_status();
        }
    }
}

ER ir_runtime_init(void)
{
    T_CMTX mutex = {0};
    T_CFLG event = {0};
    mutex.mtxatr = TA_INHERIT;
    runtime_mutex = tk_cre_mtx(&mutex);
    if (runtime_mutex < E_OK)
    {
        return runtime_mutex;
    }
    event.flgatr = TA_TFIFO;
    runtime_event = tk_cre_flg(&event);
    if (runtime_event < E_OK)
    {
        return runtime_event;
    }
    owner.failed_channels = 7u;
#if MOTION_LINE_CIRCUIT
    owner.failed_channels = line_boot_calibration(owner.calibration);
    if(owner.failed_channels==0u)owner.state=IR_RUNTIME_READY;
    /* healthy stays false until acquisition and fresh bridge publication pass. */
#endif
    published = owner;
    motion_ir_bridge_init(&bridge);
    /* Phase 1: prevent any movement before the sensor task starts. */
    set_health(false);
    return E_OK;
}

ER ir_runtime_start(void)
{
    ID task = app_start_task((FP)ir_task, IR_TASK_PRIORITY, TP_PRC1, "ir_task");
    return (task < E_OK) ? task : E_OK;
}

ER ir_runtime_calibration(ir_runtime_command_t command)
{
    if ((command != IR_RUNTIME_BEGIN) && (command != IR_RUNTIME_FINISH))
    {
        return E_PAR;
    }
    if (runtime_mutex <= 0 || runtime_event <= 0)
    {
        return E_OBJ;
    }
    ER error = lock_runtime();
    if (error < E_OK)
    {
        return error;
    }
    if ((pending.command != 0u) || pending.processing ||
        ((command == IR_RUNTIME_FINISH) && (published.state != IR_RUNTIME_CALIBRATING)))
    {
        tk_unl_mtx(runtime_mutex);
        return E_OBJ;
    }
    if (command == IR_RUNTIME_BEGIN)
    {
        /* Run the stop path before exposing this command to the other core. */
        error = motion_set_fault(MOTION_HEALTH_IR, true);
        if (error == E_OK)
        {
            error = motion_stop();
        }
    }
    if (error == E_OK)
    {
        pending.command = (uint8_t)command;
        published.command_pending = true;
        error = tk_set_flg(runtime_event, IR_COMMAND_EVENT);
    }
    tk_unl_mtx(runtime_mutex);
    return error;
}

ER ir_runtime_get_status(ir_runtime_status_t *status)
{
    if (status == NULL)
    {
        return E_PAR;
    }
    if (runtime_mutex <= 0)
    {
        return E_OBJ;
    }
    ER error = lock_runtime();
    if (error == E_OK)
    {
        *status = published;
        tk_unl_mtx(runtime_mutex);
    }
    return error;
}
