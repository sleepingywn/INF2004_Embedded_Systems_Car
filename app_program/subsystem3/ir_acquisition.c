#include "ir_acquisition.h"
#include "../../config/config_device.h"
#include "../../device/adc/sysdepend/rp2040/adc_sysdep.h"

#if DEVCNF_USE_ADC
#error "S3 owns ADC IRQ 22; disable the competing sample ADC device driver"
#endif

#define IR_TIMER_IRQ 3u /* RP2040 TIMER_IRQ_3, hardware alarm 3 */
#define IR_TIMER_BIT (1u << 3)
#define IR_FIFO_EMPTY (1u << 8)
#define IR_FIFO_ERROR (1u << 15)
#define IR_FIFO_OVER (1u << 11)
#define IR_FIFO_CONFIG ((1u << ADC_FCS_THRESH_POS) | (1u << 2) | ADC_FCS_EN)

_Static_assert(IR_NUM_CH == 3u && IR_CH_BARCODE == 0u && IR_CH_LINE_A == 1u && IR_CH_LINE_B == 2u,
               "ADC scan requires channels 0, 1, 2 in this order");
_Static_assert(IR_ACQ_RING_SIZE == 16u, "Ring indices use four bits");

static ID ready_event;
static volatile ir_raw_frame_t ring[IR_ACQ_RING_SIZE];
static volatile struct
{
    uint8_t head : 4;
    uint8_t tail : 4;
} queue;
static volatile ir_acq_status_t acquisition;
static struct
{
    ir_raw_frame_t frame;
    uint32_t deadline_us;
    uint32_t deadline_ms;
    uint32_t started_us;
    uint8_t channel : 2;
    bool active : 1;
} scan;

/* The timer is shared with B2 and the WiFi port. Never reset its counter.
 * Read the raw registers, not TIMELR/TIMEHR's shared read latch. */
static uint64_t timer_us(void)
{
    uint32_t high;
    uint32_t low;
    do
    {
        high = in_w(TIMER_TIMERAWH);
        low = in_w(TIMER_TIMERAWL);
    } while (high != in_w(TIMER_TIMERAWH));
    return ((uint64_t)high << 32) | low;
}

void ir_acquisition_stop(void)
{
    DisableInt(IR_TIMER_IRQ);
    DisableInt(INTNO_ADC);
    clr_w(TIMER_INTE, IR_TIMER_BIT);
    out_w(TIMER_ARMED, IR_TIMER_BIT); /* disarm only alarm 3 */
    out_w(TIMER_INTR, IR_TIMER_BIT);
    clr_w(TIMER_INTF, IR_TIMER_BIT);
    out_w(ADC_INTE, 0);
    out_w(ADC_CS, 0);
    acquisition.running = false;
    scan.active = false;
}

static void acquisition_fault(unsigned fault)
{
    acquisition.faults |= fault;
    ir_acquisition_stop();
    queue.tail = queue.head; /* no frames from before a fault escape afterwards */
    (void)tk_set_flg(ready_event, IR_ACQ_EVENT);
}

static void arm_deadline(void)
{
    out_w(TIMER_ALARM3, scan.deadline_us);
    /* RP2040 alarms compare for equality. A delayed write must not sleep for
     * a full 32-bit timer wrap. Force an immediate IRQ if the deadline passed. */
    if ((uint32_t)(in_w(TIMER_TIMERAWL) - scan.deadline_us) < 0x80000000u)
    {
        set_w(TIMER_INTF, IR_TIMER_BIT);
    }
}

static void timer_interrupt(UINT intno)
{
    (void)intno;
    out_w(TIMER_INTR, IR_TIMER_BIT);
    clr_w(TIMER_INTF, IR_TIMER_BIT);
    uint32_t now = in_w(TIMER_TIMERAWL);
    uint32_t late = now - scan.deadline_us;
    if (!acquisition.running)
    {
        return;
    }
    if (late > acquisition.max_lateness_us)
    {
        acquisition.max_lateness_us = late;
    }
    if ((late > IR_ACQ_MAX_JITTER_US) || scan.active)
    {
        acquisition_fault(IR_ACQ_TIMING);
        return;
    }
    if (((in_w(ADC_CS) & ADC_CS_READY) == 0u) || ((in_w(ADC_FCS) & IR_FIFO_EMPTY) == 0u))
    {
        acquisition_fault(IR_ACQ_ADC);
        return;
    }
    scan.frame.sampled_ms = scan.deadline_ms;
    scan.started_us = now;
    scan.channel = 0;
    scan.active = true;
    scan.deadline_us += IR_ACQ_PERIOD_US;
    ++scan.deadline_ms; /* full millisecond clock, independent of raw-us wrap */
    out_w(ADC_CS, ADC_CS_EN | ADC_CS_STRAT_ONCE);
    arm_deadline();
}

static void adc_interrupt(UINT intno)
{
    (void)intno;
    uint32_t fifo_status = in_w(ADC_FCS);
    if (!acquisition.running || !scan.active ||
        ((fifo_status & (IR_FIFO_EMPTY | IR_FIFO_OVER)) != 0u))
    {
        acquisition_fault(IR_ACQ_ADC);
        return;
    }
    uint32_t value = in_w(ADC_FIFO);
    uint32_t elapsed = in_w(TIMER_TIMERAWL) - scan.started_us;
    if (((value & IR_FIFO_ERROR) != 0u) || (elapsed > IR_ACQ_MAX_JITTER_US))
    {
        acquisition_fault(((value & IR_FIFO_ERROR) != 0u) ? IR_ACQ_ADC : IR_ACQ_TIMING);
        return;
    }
    switch (scan.channel)
    {
        case IR_CH_BARCODE:
            scan.frame.barcode = value & 0xfffu;
            break;
        case IR_CH_LINE_A:
            scan.frame.line_a = value & 0xfffu;
            break;
        default:
            scan.frame.line_b = value & 0xfffu;
            break;
    }
    if (scan.channel < IR_CH_LINE_B)
    {
        ++scan.channel;
        out_w(ADC_CS,
              ADC_CS_EN | ADC_CS_STRAT_ONCE | ((uint32_t)scan.channel << ADC_CS_AINSEL_POS));
        return;
    }
    scan.active = false;
    if (elapsed > acquisition.max_scan_us)
    {
        acquisition.max_scan_us = elapsed;
    }
    uint8_t next = (queue.head + 1u) & (IR_ACQ_RING_SIZE - 1u);
    if (next == queue.tail)
    {
        acquisition_fault(IR_ACQ_OVERFLOW);
        return;
    }
    ring[queue.head] = scan.frame;
    __asm__ volatile("" ::: "memory");
    queue.head = next;
    if (acquisition.samples != UINT32_MAX)
    {
        ++acquisition.samples;
    }
    (void)tk_set_flg(ready_event, IR_ACQ_EVENT);
}

static ER prepare_adc(void)
{
    /* Exclusive owner: reset the ADC, never the shared TIMER block. */
    set_w(RESETS_RESET, RESETS_RESET_ADC);
    clr_w(RESETS_RESET, RESETS_RESET_ADC);
    unsigned attempts = 10000u;
    while (((in_w(RESETS_RESET_DONE) & RESETS_RESET_ADC) == 0u) && (attempts > 0u))
    {
        --attempts;
    }
    if (attempts == 0u)
    {
        return E_SYS;
    }
    for (unsigned pin = 26u; pin <= 28u; ++pin)
    {
        out_w(GPIO_CTRL(pin), GPIO_CTRL_FUNCSEL_NULL);
        clr_w(GPIO(pin), GPIO_IE | GPIO_PUE | GPIO_PDE);
    }
    out_w(ADC_DIV, 0); /* 48 MHz ADC clock supplied by BSP; one-shot conversions */
    out_w(ADC_FCS, IR_FIFO_CONFIG);
    out_w(ADC_CS, ADC_CS_EN);
    attempts = 10000u;
    while (((in_w(ADC_CS) & ADC_CS_READY) == 0u) && (attempts > 0u))
    {
        --attempts;
    }
    return (attempts == 0u) ? E_SYS : E_OK;
}

ER ir_acquisition_start(ID event)
{
    ir_acquisition_stop();
    ready_event = event;
    queue.head = queue.tail = 0;
    acquisition = (ir_acq_status_t){0};
    if (event <= 0)
    {
        return E_PAR;
    }
    /* BSP startup establishes the shared 1 MHz timebase. Releasing reset
     * again is idempotent and preserves a running counter. */
    clr_w(RESETS_RESET, RESETS_RESET_TIMER);
    unsigned attempts = 10000u;
    while (((in_w(RESETS_RESET_DONE) & RESETS_RESET_TIMER) == 0u) && (attempts > 0u))
    {
        --attempts;
    }
    ER error = (attempts == 0u) ? E_SYS : prepare_adc();
    T_DINT definition = {0};
    definition.intatr = TA_HLNG;
    if (error == E_OK)
    {
        definition.inthdr = (FP)timer_interrupt;
        error = tk_def_int(IR_TIMER_IRQ, &definition);
    }
    if (error == E_OK)
    {
        definition.inthdr = (FP)adc_interrupt;
        error = tk_def_int(INTNO_ADC, &definition);
    }
    if (error < E_OK)
    {
        acquisition.faults = IR_ACQ_HARDWARE;
        ir_acquisition_stop();
        return error;
    }
    uint64_t next_ms = timer_us() / 1000u + 2u;
    scan.deadline_us = (uint32_t)(next_ms * 1000u);
    scan.deadline_ms = (uint32_t)next_ms;
    acquisition.running = true;
    ClearInt(IR_TIMER_IRQ);
    ClearInt(INTNO_ADC);
    out_w(ADC_INTE, 1u);
    set_w(TIMER_INTE, IR_TIMER_BIT);
    /* Same NVIC priority: neither ISR can preempt the other's packed state.
     * Both are enabled by this pinned core-0 task, never by another core. */
    EnableInt(INTNO_ADC, 2);
    EnableInt(IR_TIMER_IRQ, 2);
    arm_deadline();
    return E_OK;
}

bool ir_acquisition_pop(ir_raw_frame_t *frame, ir_acq_status_t *status)
{
    UINT interrupt_state;
    DI(interrupt_state);
    __asm__ volatile("" ::: "memory");
    *status = acquisition;
    bool available = acquisition.running && (queue.head != queue.tail);
    if (available)
    {
        *frame = ring[queue.tail];
        queue.tail = (queue.tail + 1u) & (IR_ACQ_RING_SIZE - 1u);
    }
    __asm__ volatile("" ::: "memory");
    EI(interrupt_state);
    return available;
}

void ir_acquisition_discard(void)
{
    UINT interrupt_state;
    DI(interrupt_state);
    queue.tail = queue.head;
    EI(interrupt_state);
}
