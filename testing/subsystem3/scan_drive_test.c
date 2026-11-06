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
    scan_junction_command = 'A';
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
    scan_drive_result(4, false);assert(scan_running && scan_retries==0);
    scan_drive_result(25, false);stopped();assert(scan_session && scan_phase==SCAN_WAIT_BACK);
    assert(!scan_drive_accept_samples());
    test_us+=299000;scan_drive_poll(true);stopped();
    test_us+=1000;scan_drive_poll(true);assert(scan_running && scan_phase==SCAN_REVERSE);
    assert(scan_leg_goal[0]==900 && scan_leg_goal[1]==900);
    assert(levels[4][0]>0 && levels[4][1]==0 && levels[5][1]>0 && levels[5][0]==0);
    scan_drive_result(29, false);assert(scan_running); /* reverse capture ignored */
    move_ticks(100,-1);stopped();assert(scan_reason==8);
    scan_drive_poll(true);assert(scan_phase==SCAN_WAIT_FORWARD);
    test_us+=300000;assert(scan_drive_poll(true));assert(scan_running);
    assert(scan_phase==SCAN_FORWARD && scan_drive_accept_samples());
    move_ticks(30,1);scan_drive_result(17, false);stopped();assert(scan_retries==2);
    button_up=false;test_us+=5000;scan_motor_tick(NULL);stopped();assert(!scan_session);
    test_us+=1000000;scan_drive_poll(true);stopped(); /* no auto restart after stop */

    reset_test();request_run();move_ticks(10,1);scan_drive_result(29, false);
    assert(scan_running && scan_phase == SCAN_FORWARD);
    reset_test();scan_drive_result(25, false);stopped();assert(!scan_session);
    reset_test();request_run();move_ticks(10,1);scan_drive_result(5, false);
    test_us+=300000;scan_drive_poll(true);assert(scan_phase==SCAN_REVERSE);
    test_us=scan_deadline_us;scan_motor_tick(NULL);stopped();assert(scan_reason==10);
    reset_test();
    scan_drive_start_demo();
    test_us = SCAN_DEMO_DELAY_US - 1000u;
    scan_drive_poll(true);
    stopped();
    test_us = SCAN_DEMO_DELAY_US;
    scan_drive_poll(true);
    assert(scan_phase == SCAN_DEMO_FORWARD && scan_running && scan_drive_accept_samples());
    assert(levels[4][1] > 0 && levels[5][0] > 0);
    move_ticks(50, 1); /* Old startup demo turned after one second. */
    assert(scan_running && scan_phase == SCAN_DEMO_FORWARD);
    scan_drive_observe_junction(true);
    test_us += SCAN_JUNCTION_CONFIRM_US - 1000u;
    scan_drive_observe_junction(true);
    assert(scan_running);
    scan_drive_observe_junction(false);
    scan_drive_observe_junction(true);
    move_ticks(2, 1);
    scan_drive_observe_junction(true);
    assert(scan_phase == SCAN_RIGHT_ARC && scan_running && scan_step_us == test_us);
    assert(levels[4][1] > 0 && levels[4][0] == 0);
    assert(levels[5][0] > 0 && levels[5][1] == 0);
    assert(levels[4][1] > levels[5][0]);

    /* Ignore the right edge before enough turn, and ignore the junction itself. */
    for (unsigned i = 0; i < 14; ++i) {
        test_us += 20000u;
        scan_counts[0] += 15;
        scan_counts[1] += 4;
        scan_drive_observe_line(true, i >= 10, false, i >= 10 ? 80 : 2, 80,
                                i >= 10 ? 0 : -78);
        scan_motor_tick(NULL);
        assert(scan_phase == SCAN_RIGHT_ARC && scan_running);
    }
    assert(scan_speed.left.target_cps == 750 && scan_speed.right.target_cps == 200);
    /* A right sensor reading below the old black threshold can acquire the exit. */
    for (unsigned i = 0; i < 2; ++i) {
        test_us += 20000u;
        scan_counts[0] += 15;
        scan_counts[1] += 4;
        scan_drive_observe_line(true, false, false, 5, 45, -40);
        scan_motor_tick(NULL);
        assert(scan_running);
        if (i == 0) assert(scan_phase == SCAN_RIGHT_ARC);
    }
    assert(scan_phase == SCAN_LINE_FOLLOW);
    assert(scan_speed.left.target_cps == 570 && scan_speed.right.target_cps == 330);
    assert(levels[5][1] == 0);
    test_us += 20000u;
    scan_counts[0] += 12;
    scan_counts[1] += 6;
    scan_drive_observe_line(true, false, false, 65, 5, 60);
    scan_motor_tick(NULL);
    assert(scan_speed.left.target_cps < scan_speed.right.target_cps);
    scan_drive_observe_line(true, false, true, 0, 0, 0);
    stopped();
    assert(scan_reason == 18 && !scan_session);

    reset_test();
    request_run();
    scan_drive_command('A');
    scan_counts[0] += SCAN_ARC_EITHER_SIDE_LEAD - 1;
    scan_drive_observe_line(true, false, false, 45, 5, 40);
    assert(scan_phase == SCAN_RIGHT_ARC && !scan_line_candidate);
    ++scan_counts[0];
    scan_drive_observe_line(true, false, false, 45, 5, 40);
    test_us += SCAN_EXIT_CONFIRM_US;
    scan_drive_observe_line(true, false, false, 45, 5, 40);
    assert(scan_phase == SCAN_LINE_FOLLOW);

    reset_test();
    request_run();
    scan_drive_command('A');
    assert(scan_phase == SCAN_RIGHT_ARC && scan_running);
    test_us = scan_deadline_us;
    scan_motor_tick(NULL);
    stopped();
    assert(scan_reason == 16);

    reset_test();
    request_run();
    scan_drive_command('A');
    test_us += SCAN_LINE_FRESH_US + 1u;
    scan_motor_tick(NULL);
    stopped();
    assert(scan_reason == 18);

    reset_test();
    scan_drive_start_demo();
    button_up = false;
    scan_motor_tick(NULL);
    test_us += SCAN_DEMO_DELAY_US;
    scan_drive_poll(true);
    stopped();
    assert(!scan_session);

    reset_test();
    scan_drive_start_demo();
    test_us = SCAN_DEMO_DELAY_US;
    scan_drive_poll(true);
    move_ticks(399, 1);
    test_us += 20000u;
    scan_motor_tick(NULL);
    scan_drive_poll(true);
    stopped();
    assert(!scan_session && scan_reason == 11);

    reset_test();
    request_run();
    scan_junction_command = 'B';
    scan_drive_observe_junction(true);
    test_us += SCAN_JUNCTION_CONFIRM_US;
    scan_drive_observe_junction(true);
    assert(scan_running && scan_phase == SCAN_LEFT_ARC && scan_step_us == test_us);
    assert(levels[4][0] == 0 && levels[5][1] == 0 && levels[5][0] > levels[4][1]);
    for (unsigned i = 0; i < 16; ++i) {
        test_us += 20000u;
        scan_counts[0] += 4;
        scan_counts[1] += 15;
        /* The right sensor alone cannot acquire the left exit at this angle. */
        scan_drive_observe_line(true, false, false, 5, 45, -40);
        scan_motor_tick(NULL);
        assert(scan_phase == SCAN_LEFT_ARC && scan_running);
    }
    assert(scan_speed.left.target_cps == SCAN_ARC_INNER_CPS);
    assert(scan_speed.right.target_cps == SCAN_ARC_OUTER_CPS);
    for (unsigned i = 0; i < 2; ++i) {
        test_us += 20000u;
        scan_counts[0] += 4;
        scan_counts[1] += 15;
        scan_drive_observe_line(true, false, false, 45, 5, 40);
        scan_motor_tick(NULL);
        assert(scan_running);
        if (i == 0) assert(scan_phase == SCAN_LEFT_ARC);
    }
    assert(scan_phase == SCAN_LINE_FOLLOW);
    assert(scan_speed.left.target_cps == 330 && scan_speed.right.target_cps == 570);

    reset_test();
    request_run();
    scan_drive_command('C');
    test_us += SCAN_TURN_SETTLE_US;
    scan_drive_poll(true);
    assert(scan_phase == SCAN_STRAIGHT && scan_running);
    assert(scan_leg_goal[0] == 1358 && scan_leg_goal[1] == 1358);
    assert(levels[4][1] > 0 && levels[5][0] > 0);
    move_ticks(151, 1);
    stopped();
    assert(scan_reason == 14);

    reset_test();
    request_run();
    scan_drive_command('D');
    test_us += SCAN_TURN_SETTLE_US;
    scan_drive_poll(true);
    assert(scan_phase == SCAN_TURN && scan_running);
    assert(scan_leg_goal[0] == 2667 && scan_leg_goal[1] == 2667);
    assert(levels[4][1] > 0 && levels[5][1] > 0);
    assert(scan_deadline_us - scan_step_us == 2u * SCAN_TURN_TIMEOUT_MS * 1000u);
    test_us = scan_deadline_us;
    scan_motor_tick(NULL);
    stopped();
    assert(scan_reason == 13);

    puts("PASS: right and left arcs, outgoing edge acquisition, proportional line tracing, line-loss/stale/timeout stops, straight and U-turn");
    puts("PASS: 8s scan limit, stalls, encoder faults, incomplete retry and encoder return");
}
