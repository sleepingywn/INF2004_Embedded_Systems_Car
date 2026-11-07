#include <assert.h>
#include <stdio.h>
#include "motion_host_shim.h"
#include "../../app_program/subsystem2/line_indicator.h"
#include "../../lib/libwifi/sysdepend/pico_rp2040/cyw43_led_request.h"
int main(void)
{
    line_indicator_t led={0};
    motion_status_t m={.epoch=7,.request_epoch=7,.request_id=8,.state=MOTION_IDLE,
                       .result=MOTION_COMPLETED,.updated_ms=100};
    line_indicator_observe(&led,&m,true,100);assert(led.mode==LINE_LED_ARMED);
    line_indicator_queue(&led,7,9,100);
    line_indicator_observe(&led,&m,false,100);assert(led.mode==LINE_LED_PENDING);
    m.state=MOTION_TRACING;m.result=MOTION_ACCEPTED;
    line_indicator_observe(&led,&m,false,100);assert(led.mode==LINE_LED_PENDING); /* old ID */
    m.request_id=9;m.request_epoch=6;
    line_indicator_observe(&led,&m,false,100);assert(led.mode==LINE_LED_PENDING);
    m.request_epoch=7;
    line_indicator_observe(&led,&m,false,100);assert(led.mode==LINE_LED_RUNNING);
    line_indicator_observe(&led,NULL,false,100);assert(led.mode==LINE_LED_ERROR);
    line_indicator_observe(&led,&m,false,301);assert(led.mode==LINE_LED_ERROR);
    m.updated_ms=301;line_indicator_observe(&led,&m,false,301);assert(led.mode==LINE_LED_RUNNING);
    m.state=MOTION_IDLE;m.result=MOTION_COMPLETED;
    line_indicator_observe(&led,&m,false,301);assert(led.mode==LINE_LED_OFF && !led.tracking);
    unsigned failures[]={MOTION_REJECTED,MOTION_FAILED,MOTION_CANCELLED,MOTION_EXPIRED};
    for(unsigned i=0;i<sizeof(failures)/sizeof(failures[0]);++i) {
        line_indicator_queue(&led,7,9,301);m.result=failures[i];
        line_indicator_observe(&led,&m,false,301);assert(led.mode==LINE_LED_ERROR && led.failed);
    }
    line_indicator_queue(&led,7,9,301);m.result=MOTION_ACCEPTED;m.state=MOTION_TRACING;m.faults=1;
    line_indicator_observe(&led,&m,false,301);assert(led.mode==LINE_LED_ERROR);
    m.faults=0;line_indicator_queue(&led,6,9,301);
    line_indicator_observe(&led,&m,false,301);assert(led.mode==LINE_LED_ERROR);
    line_indicator_clear(&led);line_indicator_observe(&led,&m,false,301);
    assert(led.mode==LINE_LED_OFF); /* explicit stop can't resurrect a stale running indication */
    line_indicator_queue(&led,7,10,UINT32_MAX-500u);m.updated_ms=499;
    line_indicator_observe(&led,&m,false,499);assert(led.mode==LINE_LED_ERROR); /* wrapped queue timeout */
    for(uint32_t t=0;t<2000;t+=100) {
        assert(line_indicator_level(LINE_LED_RUNNING,t));
        assert(!line_indicator_level(LINE_LED_OFF,t));
        assert(line_indicator_level(LINE_LED_ARMED,t)==(t%1000<500));
        assert(line_indicator_level(LINE_LED_ERROR,t)==(t%200==0));
    }
    assert(line_indicator_level(LINE_LED_PENDING,0) && !line_indicator_level(LINE_LED_PENDING,100));
    assert(line_indicator_level(LINE_LED_PENDING,200) && !line_indicator_level(LINE_LED_PENDING,300));
    uint32_t starts[]={0,UINT32_MAX-200u,0x7fffff00u};
    for(unsigned i=0;i<3;++i) {
        uint32_t t=starts[i],word=cyw43_led_word(t,true);
        assert(cyw43_led_level(word,t) && cyw43_led_level(word,t+500u));
        assert(!cyw43_led_level(word,t+501u));
        assert(!cyw43_led_level(cyw43_led_word(t,false),t));
    }
    puts("PASS: LED shows actual matching acceptance only, rejection/fault/stale/epoch/STOP/completion, patterns and expiring cross-core request with wrap");
    return 0;
}
