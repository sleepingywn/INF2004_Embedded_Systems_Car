#include "motion_ir_bridge.h"
#include <string.h>

void motion_ir_bridge_init(motion_ir_bridge_t *bridge)
{
    memset(bridge, 0, sizeof(*bridge));
    line_init(&bridge->tracker);
    bc_init(&bridge->capture);
}

ER motion_ir_bridge_sample(motion_ir_bridge_t *bridge, const uint8_t normalized[3], bool valid,
                           uint32_t sample_ms, uint32_t epoch)
{
    line_msg_t line = {0};
    bc_msg_t barcode = {0};
    line_state_t line_state = LINE_FAULT;
    if ((bridge == NULL) || (normalized == NULL))
    {
        return E_PAR;
    }
    bool changed_epoch = bridge->epoch != epoch;
    bool gap = bridge->primed && ((uint32_t)(sample_ms - bridge->previous_ms) != 1u);
    if (changed_epoch || gap)
    {
        motion_ir_bridge_init(bridge);
    }
    for (unsigned i = 0; i < 3u; ++i)
    {
        if (normalized[i] > 100u)
        {
            valid = false;
        }
    }
    bridge->epoch = epoch;
    bridge->previous_ms = sample_ms;
    bridge->primed = 1;
    line.t_ms = sample_ms & 0x7fffu;
    line.seq = bridge->sequence;
    bool completed = false;
    if (!valid || gap)
    {
        bc_init(&bridge->capture);
        line_init(&bridge->tracker);
        line.fault = 1;
        line.state = LINE_FAULT;
    }
    else
    {
        line.err = line_update(&bridge->tracker, normalized[IR_CH_LINE_A], normalized[IR_CH_LINE_B],
                               1u, &line_state);
        line.state = line_state;
        completed = bc_feed(&bridge->capture, normalized[IR_CH_BARCODE], &barcode);
        line.bc_active = bc_active(&bridge->capture);
    }
    ER error = E_OK;
    if ((sample_ms - bridge->line_ms >= 10u) || line.fault || completed || changed_epoch)
    {
        error = motion_publish_line(&line, sample_ms, epoch);
        if (error < E_OK)
        {
            bc_init(&bridge->capture);
            (void)motion_set_fault(MOTION_HEALTH_IR, true);
            return error;
        }
        bridge->sequence = (bridge->sequence + 1u) & 0xfu;
        bridge->line_ms = sample_ms;
    }
    if (line.fault)
    {
        error = motion_set_fault(MOTION_HEALTH_IR, true);
        return (error < E_OK) ? error : E_IO;
    }
    if (completed)
    {
        error = motion_publish_barcode(&barcode, sample_ms, epoch);
    }
    /* A full mailbox explicitly rejects this barcode; never replay it later. */
    return error;
}
