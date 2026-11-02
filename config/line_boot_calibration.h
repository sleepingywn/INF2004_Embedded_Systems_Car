#ifndef LINE_BOOT_CALIBRATION_H
#define LINE_BOOT_CALIBRATION_H
#include "../app_program/subsystem3/ir_types.h"

/* Only the explicit autostart image loads these measured references.
 * 2026-11-02 18:45 white / 18:47 black, all eyes at operating height.
 * White maxima and black minima from the captured stationary snapshots.
 * These are reproducible measured endpoints, NOT a dump of the earlier
 * in-RAM min/max accumulator (which also sampled hand-positioning transitions).
 * Recalibrate/rebuild if mounting height, surface, lighting or hardware changes.
 */
static inline uint8_t line_boot_calibration(ir_cal_t cal[IR_NUM_CH])
{
    const uint16_t white[IR_NUM_CH]={183u,196u,192u};
    const uint16_t black[IR_NUM_CH]={2985u,2575u,2345u};
    uint8_t failed=0;
    for(unsigned i=0;i<IR_NUM_CH;++i) {
        cal[i].white=white[i];cal[i].black=black[i];
        if(black[i]<=white[i] || black[i]>4095u || (uint32_t)(black[i]-white[i])<IR_CAL_MIN_CONTRAST)
            failed|=(uint8_t)(1u<<i);
    }
    return failed;
}
#endif
