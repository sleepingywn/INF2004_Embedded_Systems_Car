#include <assert.h>
#include <stdio.h>
#include "../../config/line_boot_calibration.h"
#include "../../app_program/subsystem3/sensor_line.h"
int main(void) {
    ir_cal_t cal[IR_NUM_CH];assert(line_boot_calibration(cal)==0);
    const uint16_t white_min[]={162,165,160},white_max[]={183,196,192};
    const uint16_t black_min[]={2985,2575,2345},black_max[]={3011,2601,2373};
    for(unsigned i=0;i<IR_NUM_CH;++i) {
        assert(ir_normalise(&cal[i],white_min[i])==0);
        assert(ir_normalise(&cal[i],white_max[i])==0);
        assert(ir_normalise(&cal[i],black_min[i])==100);
        assert(ir_normalise(&cal[i],black_max[i])==100);
    }
    line_tracker_t tracker;line_state_t state;line_init(&tracker);
    assert(line_update(&tracker,100,100,1,&state)==0 && state==LINE_BOTH_BLACK);
    assert(line_update(&tracker,100,0,1,&state)==100 && state==LINE_EDGE_ONLY);
    assert(line_update(&tracker,0,100,1,&state)==-100 && state==LINE_EDGE_ONLY);
    puts("PASS: measured boot references cover captured white/black ranges with actual sensor normalization/steering");
}
