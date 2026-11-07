#ifndef LINE_BOOT_CALIBRATION_H
#define LINE_BOOT_CALIBRATION_H
#include "../app_program/subsystem3/ir_types.h"

/* Only the explicit battery circuit images load these measured references.
 * GP26 retains the Nov 2 barcode reference. GP27/28 use the medians of eight
 * stationary white and black samples at operating height on Nov 3:
 * .local/gp20-recal-white-20261103-221840.json and
 * .local/gp20-recal-black-20261103-221932.json (host-clock evidence names).
 * These are reproducible measured endpoints, NOT a dump of the earlier
 * in-RAM min/max accumulator (which also sampled hand-positioning transitions).
 * Recalibrate/rebuild if mounting height, surface, lighting or hardware changes.
 */
static inline uint8_t line_boot_calibration(ir_cal_t cal[IR_NUM_CH])
{
    const uint16_t white[IR_NUM_CH]={183u,173u,160u};
    const uint16_t black[IR_NUM_CH]={2985u,2623u,2533u};
    uint8_t failed=0;
    for(unsigned i=0;i<IR_NUM_CH;++i) {
        cal[i].white=white[i];cal[i].black=black[i];
        if(black[i]<=white[i] || black[i]>4095u || (uint32_t)(black[i]-white[i])<IR_CAL_MIN_CONTRAST)
            failed|=(uint8_t)(1u<<i);
    }
    return failed;
}
#endif
