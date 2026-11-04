/* Host simulation of the actual button/drive timer; no hardware is driven. */
#include <assert.h>
#include "ir_line_bench/scan_drive.h"
static void poll_at(unsigned ms,bool up)
{test_us=ms*1000;button_up=up;scan_drive_poll(true);}
static void request_run(void)
{
    poll_at(test_us/1000+1,true);
    poll_at(test_us/1000+31,true);
    poll_at(test_us/1000+1,false);
    poll_at(test_us/1000+31,false);
    poll_at(test_us/1000+1,true);
    poll_at(test_us/1000+31,true);
    assert(scan_running);
    assert(levels[4][0]==0 && levels[4][1]>0);
    assert(levels[5][0]>0 && levels[5][1]==0);
}
static void stopped(void)
{
    assert(!scan_running);
    assert(!(levels[4][0]|levels[4][1]|levels[5][0]|levels[5][1]));
}
static void reset_test(void)
{
    scan_running=false;scan_session=false;scan_phase=SCAN_IDLE;scan_reason=0;
    scan_counts[0]=scan_counts[1]=scan_invalid=0;test_us=0;button_up=true;
    assert(scan_drive_init());stopped();
}
static void move_ticks(unsigned ticks,int direction)
{
    for(unsigned i=0;i<ticks;++i) {
        test_us+=20000;
        scan_counts[0]+=(uint32_t)(9*direction);scan_counts[1]+=(uint32_t)(9*direction);
        scan_motor_tick(NULL);
    }
}
int main(void)
{
    setbuf(stdout,NULL);
    reset_test();request_run();
    move_ticks(399,1);assert(scan_running);
    test_us+=20000;scan_motor_tick(NULL);stopped();assert(scan_reason==1);
    scan_drive_poll(true);assert(scan_phase==SCAN_WAIT_RESULT);
    test_us+=2000000;scan_drive_poll(true);stopped();assert(!scan_session);

    reset_test();request_run();
    button_up=false;test_us+=5000;scan_motor_tick(NULL);stopped();assert(scan_reason==2);
    scan_drive_poll(true);
    for(unsigned i=0;i<10;++i){test_us+=50000;scan_drive_poll(true);stopped();}

    reset_test();request_run();
    test_us+=420000;scan_motor_tick(NULL);stopped();assert(scan_reason==4);
    reset_test();request_run();++scan_invalid;test_us+=20000;
    scan_motor_tick(NULL);stopped();assert(scan_reason==3);

    reset_test();request_run();move_ticks(100,1);assert(scan_running);
    scan_drive_result(4,false);assert(scan_running && scan_retries==0);
    scan_drive_result(25,false);stopped();assert(scan_session && scan_phase==SCAN_WAIT_BACK);
    assert(!scan_drive_accept_samples());
    test_us+=299000;scan_drive_poll(true);stopped();
    test_us+=1000;scan_drive_poll(true);assert(scan_running && scan_phase==SCAN_REVERSE);
    assert(scan_back_goal[0]==900 && scan_back_goal[1]==900);
    assert(levels[4][0]>0 && levels[4][1]==0 && levels[5][1]>0 && levels[5][0]==0);
    scan_drive_result(29,false);assert(scan_running); /* reverse capture ignored */
    move_ticks(100,-1);stopped();assert(scan_reason==8);
    scan_drive_poll(true);assert(scan_phase==SCAN_WAIT_FORWARD);
    test_us+=300000;assert(scan_drive_poll(true));assert(scan_running);
    assert(scan_phase==SCAN_FORWARD && scan_drive_accept_samples());
    move_ticks(30,1);scan_drive_result(17,false);stopped();assert(scan_retries==2);
    button_up=false;test_us+=5000;scan_motor_tick(NULL);stopped();assert(!scan_session);
    test_us+=1000000;scan_drive_poll(true);stopped(); /* no auto restart after stop */

    reset_test();request_run();move_ticks(10,1);scan_drive_result(29,false);
    stopped();assert(!scan_session && scan_reason==9);
    reset_test();scan_drive_result(25,false);stopped();assert(!scan_session);
    reset_test();request_run();move_ticks(10,1);scan_drive_result(5,false);
    test_us+=300000;scan_drive_poll(true);assert(scan_phase==SCAN_REVERSE);
    test_us=scan_deadline_us;scan_motor_tick(NULL);stopped();assert(scan_reason==10);
    puts("PASS: 8s limit, button/held stop, stalls, encoder faults, 5-28 retry gate, encoder return, reverse capture suppression, repeated retries, 29-element stop, reverse timeout");
}
