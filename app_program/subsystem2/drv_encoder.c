#include "drv_encoder.h"
#include "motion_config.h"
#include "motion_logic.h"

#define ENCODER_IRQ 13u
#define GPIO_INTR0 (IO_BANK0_BASE + 0xf0u)
#define GPIO_PROC0_INTE0 (IO_BANK0_BASE + 0x100u)
#define GPIO_PROC1_INTE0 (IO_BANK0_BASE + 0x130u)
#define EDGE_MASK(pin) (12u << ((pin) * 4u))
#define ALL_EDGES                                                                                  \
    (EDGE_MASK(MOTION_ENCODER_LEFT_A) | EDGE_MASK(MOTION_ENCODER_LEFT_B) |                         \
     EDGE_MASK(MOTION_ENCODER_RIGHT_A) | EDGE_MASK(MOTION_ENCODER_RIGHT_B))
_Static_assert(MOTION_ENCODER_LEFT_A == 4u && MOTION_ENCODER_LEFT_B == 5u &&
                   MOTION_ENCODER_RIGHT_A == 2u && MOTION_ENCODER_RIGHT_B == 3u,
               "Review IRQ register allocation when changing encoder pins");

static volatile encoder_raw_t counts;
static struct
{
    uint8_t left : 2;
    uint8_t right : 2;
} previous;

static uint8_t read_pair(uint32_t gpio, unsigned a, unsigned b)
{
    return (uint8_t)((((gpio >> a) & 1u) << 1) | ((gpio >> b) & 1u));
}

static uint8_t count_pair(uint32_t gpio, uint32_t edges, unsigned a, unsigned b, uint8_t last,
                          volatile uint32_t *count)
{
    uint8_t current = read_pair(gpio, a, b);
    int8_t delta = motion_quadrature_delta(last, current);
    uint32_t pair_edges = edges & (EDGE_MASK(a) | EDGE_MASK(b));
    if (pair_edges == 0u)
    {
        return last;
    }
    /* More than one pending edge has ambiguous ordering. Do not invent counts.
     * Bounded ISR x4 capture must be measured against the real encoder rate. */
    if ((delta == 2) || ((pair_edges & (pair_edges - 1u)) != 0u) ||
        ((pair_edges != 0u) && (delta == 0)))
    {
        if (counts.invalid != UINT32_MAX)
        {
            ++counts.invalid;
        }
    }
    else if (delta > 0)
    {
        ++*count;
    }
    else if (delta < 0)
    {
        --*count; /* intentional unsigned modulo wrap */
    }
    return current;
}

static void encoder_interrupt(UINT intno)
{
    uint32_t edges = in_w(GPIO_INTR0) & ALL_EDGES;
    out_w(GPIO_INTR0, edges); /* W1C only our latched edges */
    uint32_t gpio = in_w(GPIO_IN);
    previous.left = count_pair(gpio, edges, MOTION_ENCODER_LEFT_A, MOTION_ENCODER_LEFT_B,
                               previous.left, &counts.left);
    previous.right = count_pair(gpio, edges, MOTION_ENCODER_RIGHT_A, MOTION_ENCODER_RIGHT_B,
                                previous.right, &counts.right);
    (void)intno;
}

ER encoder_hw_init(void)
{
    const UINT pins[4] = {MOTION_ENCODER_LEFT_A, MOTION_ENCODER_LEFT_B, MOTION_ENCODER_RIGHT_A,
                          MOTION_ENCODER_RIGHT_B};
    T_DINT definition = {0};
    definition.intatr = TA_HLNG;
    definition.inthdr = (FP)encoder_interrupt;
    DisableInt(ENCODER_IRQ);
    /* IO_BANK0 handler is owned by B2; WiFi port currently polls its GPIOs.
     * Any future GPIO IRQ user needs a shared dispatcher, not another handler. */
    ER error = tk_def_int(ENCODER_IRQ, &definition);
    if (error < E_OK)
    {
        return error;
    }
    for (unsigned i = 0; i < 4u; ++i)
    {
        gpio_set_pin(pins[i], GPIO_MODE_IN);
    }
    counts.left = counts.right = counts.invalid = 0;
    uint32_t gpio = in_w(GPIO_IN);
    previous.left = read_pair(gpio, pins[0], pins[1]);
    previous.right = read_pair(gpio, pins[2], pins[3]);
    clr_w(GPIO_PROC0_INTE0, ALL_EDGES);
    clr_w(GPIO_PROC1_INTE0, ALL_EDGES);
    out_w(GPIO_INTR0, ALL_EDGES);
#if TK_SUPPORT_SMP
    set_w(GPIO_PROC1_INTE0, ALL_EDGES); /* motion task pinned to core 1 */
#else
    set_w(GPIO_PROC0_INTE0, ALL_EDGES);
#endif
    ClearInt(ENCODER_IRQ);
    EnableInt(ENCODER_IRQ, 2);
    return E_OK;
}

void encoder_hw_snapshot(encoder_raw_t *snapshot)
{
    UINT interrupt_state;
    DI(interrupt_state);
    __asm__ volatile("" ::: "memory");
    snapshot->left = counts.left;
    snapshot->right = counts.right;
    snapshot->invalid = counts.invalid;
    __asm__ volatile("" ::: "memory");
    EI(interrupt_state);
}
