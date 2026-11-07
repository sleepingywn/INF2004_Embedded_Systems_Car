#include "../../app_program/subsystem2/startup_gate.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
 motion_startup_decision_t d;
 d=motion_startup_decide(100,100,false,true);assert(!d.release&&!d.timeout);
 d=motion_startup_decide(100,5100,false,true);assert(!d.release&&!d.timeout);
 d=motion_startup_decide(100,5101,false,true);assert(!d.release&&d.timeout);
 d=motion_startup_decide(100,200,true,true);assert(d.release&&!d.timeout);
 d=motion_startup_decide(100,5101,true,true);assert(d.release&&d.timeout);
 d=motion_startup_decide(UINT32_MAX-99,100,true,true);assert(d.release&&!d.timeout);
 d=motion_startup_decide(UINT32_MAX-99,5001,false,true);assert(!d.release&&d.timeout);
 d=motion_startup_decide(100,200,true,false);assert(!d.release&&!d.timeout);
 d=motion_startup_decide(100,5101,true,false);assert(!d.release&&d.timeout);
 puts("Startup release, deadline and clock-wrap policy: PASS");return 0;
}
