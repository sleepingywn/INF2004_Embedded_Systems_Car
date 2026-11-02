#include <assert.h>
#include <stdio.h>
#include "../../app_program/subsystem2/line_button.h"
int main(void) {
    line_button_t b={0};
    assert(line_button_step(&b,0,true)==LINE_BUTTON_NONE);
    uint32_t start=UINT32_MAX-20u;
    line_button_arm(&b,start,true);
    assert(line_button_step(&b,start+100u,true)==LINE_BUTTON_NONE);
    assert(line_button_step(&b,start+110u,false)==LINE_BUTTON_NONE);
    assert(line_button_step(&b,start+140u,false)==LINE_BUTTON_NONE);
    assert(b.ready);
    /* A brief pulse/bounce is not a start. */
    assert(line_button_step(&b,start+150u,true)==LINE_BUTTON_NONE);
    assert(line_button_step(&b,start+155u,false)==LINE_BUTTON_NONE);
    assert(line_button_step(&b,start+190u,false)==LINE_BUTTON_NONE);
    assert(line_button_step(&b,start+200u,true)==LINE_BUTTON_NONE);
    assert(line_button_step(&b,start+230u,true)==LINE_BUTTON_NONE);
    assert(line_button_step(&b,start+240u,false)==LINE_BUTTON_NONE);
    assert(line_button_step(&b,start+270u,false)==LINE_BUTTON_START);
    assert(line_button_step(&b,start+280u,false)==LINE_BUTTON_NONE);
    assert(line_button_step(&b,start+281u,true)==LINE_BUTTON_STOP);
    for(unsigned i=0;i<100;i++)assert(line_button_step(&b,start+300u+i,(i&1)!=0)==LINE_BUTTON_NONE);
    puts("PASS: held button, debounce/bounce, time wrap, one-shot start, immediate press stop and no replay");
}
