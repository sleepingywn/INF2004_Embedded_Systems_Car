/* Host tests for MOTION_LINE_ROUTE (barcode -> junction turn -> V5).
 * Part 1: pure logic in motion_route.h / motion_route_decode.h.
 * Part 2: the real motion.c owner loop with a fake kernel, clock, motors and
 *         encoders whose counts follow the commanded duty.
 * Build: see testing/subsystem2/run_route_tests.py. No hardware access. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include "motion_host_shim.h"
#include "../../app_program/subsystem2/motion.c"
#undef memset
#undef memcpy

/* ---------------- Part 1: pure route logic ---------------- */
static bc_msg_t make_code(const char *character, bool reverse, unsigned narrow, unsigned wide)
{
    const char *g[3] = {CODE39_STAR, character, CODE39_STAR};
    unsigned w[29];
    for (int k = 0; k < 3; k++) {
        for (int i = 0; i < 9; i++) w[k * 10 + i] = g[k][i] == 'W' ? wide : narrow;
        if (k < 2) w[k * 10 + 9] = narrow;
    }
    bc_msg_t m = {0};
    m.hdr.count = 29;
    for (int i = 0; i < 29; i++) m.widths[i] = (bc_width_t)(reverse ? w[28 - i] : w[i]);
    return m;
}
static void logic_tests(void)
{
    /* Every Code 39 character in both directions; A vs 1/K/U kept apart. */
    unsigned n = sizeof code39_table / sizeof code39_table[0];
    for (unsigned i = 0; i < n; i++)
        for (int r = 0; r < 2; r++) {
            bc_msg_t m = make_code(code39_table[i].p, r, 100, 300);
            assert(route_code39_decode(&m) == code39_table[i].code);
        }
    /* 16-bit widths needed: the big barcode's wide gaps exceed 255 ms. */
    bc_msg_t big = make_code("WNNNNWNNW", true, 160, 900);
    assert(route_code39_decode(&big) == 'A');
    /* 8 Nov track captures (time widths, ms) from the bench. */
    unsigned z[29] = {71,110,287,103,299,116,65,291,79,112,79,110,85,101,261,115,242,317,67,112,76,96,234,82,250,96,62,279,63};
    unsigned a[29] = {91,184,371,203,385,204,104,448,96,178,388,258,78,409,99,162,81,201,432,138,99,158,357,173,433,171,95,423,90};
    bc_msg_t m = {0};
    m.hdr.count = 29;
    for (int i = 0; i < 29; i++) m.widths[i] = (bc_width_t)z[i];
    assert(route_code39_decode(&m) == 'Z');
    for (int i = 0; i < 29; i++) m.widths[i] = (bc_width_t)a[i];
    assert(route_code39_decode(&m) == 'A');
    m.hdr.count = 27;
    assert(route_code39_decode(&m) == 0);

    assert(route_choose('A', 'A') == 'A' && route_choose('A', 0) == 'A' &&
           route_choose(0, 'Z') == 'Z' && route_choose('A', 'B') == 0 && route_choose(0, 0) == 0);
    assert(route_letter_armable('A') && route_letter_armable('Z') && !route_letter_armable('E'));
    assert(route_first_phase('A') == ROUTE_ARC_RIGHT && route_first_phase('B') == ROUTE_ARC_LEFT &&
           route_first_phase('Z') == ROUTE_ARC_LEFT && route_first_phase('C') == ROUTE_SETTLE_BEFORE &&
           route_first_phase('D') == ROUTE_SETTLE_BEFORE);

    /* Odometry interpolation and distance widths, including wraparound. */
    static route_odometry_t o;
    route_odo_reset(&o);
    for (uint32_t t = 0; t <= 4000; t += 20) route_odo_add(&o, 1000 + t, 0xFFFFF000u + t / 2u);
    uint32_t v;
    assert(route_odo_at(&o, 1010, &v) && v == 0xFFFFF000u + 5u);
    assert(route_odo_at(&o, 5000, &v) && v == 0xFFFFF000u + 2000u);
    assert(route_odo_at(&o, 5030, &v));     /* up to 40 ms past newest */
    assert(!route_odo_at(&o, 5100, &v));
    assert(!route_odo_at(&o, 900, &v));     /* older than history */
    bc_msg_t t_msg = make_code("NNWNNWNNW", false, 40, 120), d_msg;
    uint32_t end = 4800, total = 0;
    for (int i = 0; i < 29; i++) total += t_msg.widths[i];
    assert(end - total > 1000);
    assert(route_distance_widths(&o, &t_msg, end, &d_msg));
    for (int i = 0; i < 29; i++) assert(d_msg.widths[i] * 2 >= t_msg.widths[i] - 1 && d_msg.widths[i] * 2 <= t_msg.widths[i] + 1);
    assert(route_code39_decode(&d_msg) == 'B');
    assert(!route_distance_widths(&o, &t_msg, 99999, &d_msg));
    /* History is bounded: oldest entries drop out. */
    for (uint32_t t = 4020; t <= 4000 + 20 * ROUTE_ODO_SIZE; t += 20) route_odo_add(&o, 1000 + t, t);
    assert(!route_odo_at(&o, 1000, &v));

    /* Ken's exit rule. Right turn: right sensor after 150 counts of lead. */
    assert(!route_exit_sighting(true, 100, 0, 5, 45, false));   /* too early */
    assert(route_exit_sighting(true, 200, 40, 5, 45, false));
    assert(!route_exit_sighting(true, 200, 40, 5, 45, true));   /* on the crossing */
    assert(!route_exit_sighting(true, 200, 40, 45, 5, false));  /* wrong side early */
    assert(route_exit_sighting(true, 400, 40, 45, 5, false));   /* either side after 300 */
    assert(route_exit_sighting(false, 40, 200, 45, 5, false));  /* left turn: left sensor */
    assert(!route_exit_sighting(false, 40, 200, 30, 5, false)); /* below darkness 35 */
    assert(!route_exit_sighting(false, 40, 200, 40, 35, false)); /* contrast < 10 */
    puts("PASS route logic: A-Z decode both directions, 16-bit widths, track captures, choose rule, "
         "odometry/distance widths with wrap, Ken's exit rule");
}

/* ---------------- Part 2: motion.c owner loop ---------------- */
static jmp_buf done;
static uint32_t clock_ms, flag_bits, stop_at_ms;
static int out_left, out_right; /* electrical duty as written */
static encoder_raw_t fake_raw;
static int scenario;
static bool sent;
static int max_phase_seen;
static unsigned carried_handovers;
static char route_seen[64];
static unsigned event_count;
UW in_w(UW reg) { uint64_t t = (uint64_t)clock_ms * 1000; return reg == TIMER_TIMERAWH ? (UW)(t >> 32) : (UW)t; }
UW tm_usb_state(void) { return 3; }
ER tk_loc_mtx(ID a, int b) { (void)a; (void)b; return E_OK; }
ER tk_unl_mtx(ID a) { (void)a; return E_OK; }
ER tk_set_flg(ID a, UINT b) { (void)a; flag_bits |= b; return E_OK; }
ER tk_clr_flg(ID a, UINT b) { (void)a; flag_bits &= b; return E_OK; }
ER tk_ref_flg(ID a, T_RFLG *b) { (void)a; b->flgptn = flag_bits; return E_OK; }
ID tk_cre_mtx(const T_CMTX *a) { (void)a; return 1; }
ID tk_cre_flg(const T_CFLG *a) { (void)a; return 1; }
ID tk_cre_cyc(const T_CCYC *a) { (void)a; return 1; }
ID app_start_task(FP a, int b, int c, const char *d) { (void)a; (void)b; (void)c; (void)d; return 1; }
ER tk_sta_cyc(ID a) { (void)a; return E_OK; }
ER tk_dly_tsk(unsigned a) { (void)a; return E_OK; }
ER motor_hw_init(void) { return E_OK; }
ER encoder_hw_init(void) { return E_OK; }
void encoder_hw_snapshot(encoder_raw_t *r) { *r = fake_raw; }
void motor_hw_stop(void) { out_left = out_right = 0; }
void motor_hw_write(int16_t l, int16_t r)
{
    assert(l >= -200 && l <= 200 && r >= -200 && r <= 200);
    out_left = l; out_right = r;
}
static void drain_events(void)
{
    motion_route_event_t e;
    while (motion_route_next_event(&e)) {
        if (event_count < sizeof route_seen - 1) route_seen[event_count++] = (char)('0' + e.kind);
        if (e.kind == ROUTE_EV_FAILED) printf("  route failed reason=%d phase=%d\n", e.v[0], e.v[1]);
    }
}
/* Line seen by the car. Phases of the scripted track per scenario. */
static void publish_line(void)
{
    line_msg_t line = {.state = LINE_EDGE_ONLY, .err = 90};
    uint16_t levels = line_levels_pack(95, 5);
    bool junction = clock_ms >= 600 && clock_ms < 700;
    if (junction) { line = (line_msg_t){.state = LINE_BOTH_BLACK}; levels = line_levels_pack(90, 90); }
    if (route.phase == ROUTE_ARC_RIGHT || route.phase == ROUTE_ARC_LEFT) {
        bool right = route.phase == ROUTE_ARC_RIGHT;
        int32_t lead = right ? motion_count_delta(route_fwd_left(), route.origin_left) -
                               motion_count_delta(route_fwd_right(), route.origin_right)
                             : motion_count_delta(route_fwd_right(), route.origin_right) -
                               motion_count_delta(route_fwd_left(), route.origin_left);
        if (scenario == 6 || lead < 200) { line = (line_msg_t){.state = LINE_LOST}; levels = line_levels_pack(3, 3); }
        else if (right) { line = (line_msg_t){.state = LINE_EDGE_ONLY, .err = -40}; levels = line_levels_pack(5, 45); }
        else { line = (line_msg_t){.state = LINE_EDGE_ONLY, .err = 40}; levels = line_levels_pack(45, 5); }
    }
    shared.line = line; shared.have_line = true; shared.line_ms = clock_ms;
    shared.line_epoch = state.epoch; shared.line_levels = levels;
}
ER tk_wai_flg(ID a, UINT b, UINT c, UINT *d, int e)
{
    (void)a; (void)b; (void)c; (void)e;
    drain_events();
    static route_phase_t previous_phase;
    if (clock_ms == 0) previous_phase = ROUTE_IDLE;
    /* Handover after an arc keeps the arc's motor power (bench W1 fix). */
    if ((previous_phase == ROUTE_ARC_RIGHT || previous_phase == ROUTE_ARC_LEFT) && route.phase == ROUTE_IDLE &&
        !state.faults) {
        assert(line_trial.speed.left.duty == route.speed.left.duty &&
               line_trial.speed.right.duty == route.speed.right.duty &&
               (line_trial.speed.left.duty > 80u || line_trial.speed.right.duty > 80u));
        ++carried_handovers;
    }
    previous_phase = route.phase;
    if ((int)route.phase > max_phase_seen) max_phase_seen = (int)route.phase;
    if (clock_ms >= stop_at_ms) longjmp(done, 1);
    clock_ms += 20;
    /* Encoders follow the electrical output: forward duty 200 -> 20 counts / 20 ms. */
    int left_fwd = -out_left, right_fwd = out_right;
    fake_raw.left -= (uint32_t)(left_fwd / 10);
    fake_raw.right += (uint32_t)(right_fwd / 10);
    publish_line();
    if (clock_ms == 40) {
        motion_request_t request;
        assert(motion_request_init(&request, MOTION_LINE_TRACE, MOTION_LOCAL, state.epoch, 1,
                                   LINE_FOLLOW_MAX_MS, 0, 0, 0) == E_OK);
        assert(motion_submit(&request) == E_OK);
        sent = true;
    }
    /* A barcode completes at 400 ms (scenario 5: none). */
    if (clock_ms == 400 && scenario != 5) {
        const char *letter = scenario == 1 || scenario == 6 ? "WNNNNWNNW" : scenario == 2 ? "WNWNNWNNW" :
                             scenario == 3 ? "NNNNWWNNW" : scenario == 4 ? "NWWNWNNNN" : "WNNNWWNNN";
        if (scenario == 2) letter = "WNWNNWNNN"; /* C */
        bc_msg_t bc = make_code(letter, false, 3, 9); /* short widths: fits the 400 ms history */
        shared.barcode = bc; shared.have_barcode = true;
        shared.barcode_ms = clock_ms; shared.barcode_epoch = state.epoch;
    }
    *d = flag_bits;
    return E_OK;
}
static void reset_owner(int which, uint32_t until)
{
    scenario = which; stop_at_ms = until;
    clock_ms = flag_bits = 0; out_left = out_right = 0; fake_raw = (encoder_raw_t){0};
    sent = false; max_phase_seen = 0; carried_handovers = 0; event_count = 0; memset(route_seen, 0, sizeof route_seen);
    memset(&shared, 0, sizeof(shared)); memset(&owner_flags, 0, sizeof(owner_flags));
    memset(&state, 0, sizeof(state)); memset(&diagnostics, 0, sizeof(diagnostics));
    memset(&active, 0, sizeof(active)); memset(wheels, 0, sizeof(wheels));
    previous_counts = (encoder_raw_t){0}; output_rest_until = last_invalid = owner_rejections = 0;
    stop_generation = fault_generation = handled_stop_generation = handled_fault_generation = 0;
    route_odo_reset(&route_odo); route_reset(); route_log_head = route_log_tail = 0;
    line_history_clear(0, 0);
    reset_control();
    assert(motion_init() == E_OK && motion_application_ready() == E_OK);
}
static void run(void) { if (!setjmp(done)) motion_task(0, NULL); drain_events(); }
static bool has(char kind) { return strchr(route_seen, '0' + kind) != NULL; }
static void owner_tests(void)
{
    /* 1: A -> right arc -> exit -> V5 keeps driving (carried duty). */
    reset_owner(1, 2000); run();
    assert(sent && !state.faults && owner_flags.active_valid && active.command == MOTION_LINE_TRACE);
    assert(has(ROUTE_EV_BARCODE) && has(ROUTE_EV_ARMED) && has(ROUTE_EV_JUNCTION) && has(ROUTE_EV_EXIT));
    assert(max_phase_seen >= ROUTE_ARC_RIGHT && route.phase == ROUTE_IDLE && !route.armed);
    assert(diagnostics.line_active && state.barcode == 1 && carried_handovers == 1);
    puts("PASS route owner 1: barcode A arms, junction starts the right arc, exit hands over to V5");

    /* 4: Z -> left arc -> V5. */
    reset_owner(4, 2000); run();
    assert(!state.faults && has(ROUTE_EV_ARMED) && has(ROUTE_EV_EXIT) && max_phase_seen == ROUTE_ARC_LEFT);
    puts("PASS route owner 4: barcode Z arms a left arc");

    /* 2: C -> stop, straight 1358 counts, stop, V5 restarts. */
    reset_owner(2, 6000); run();
    assert(!state.faults && has(ROUTE_EV_LEG_DONE) && has(ROUTE_EV_RESUMED) && route.phase == ROUTE_IDLE);
    assert(max_phase_seen >= ROUTE_STRAIGHT && diagnostics.line_active);
    puts("PASS route owner 2: barcode C drives straight 100 mm, then V5 resumes");

    /* 3: D -> pivot (right wheel reverses) 2667 counts each, V5 restarts. */
    reset_owner(3, 9000); run();
    assert(!state.faults && has(ROUTE_EV_LEG_DONE) && has(ROUTE_EV_RESUMED) && max_phase_seen >= ROUTE_PIVOT);
    puts("PASS route owner 3: barcode D pivots 180 deg, then V5 resumes");

    /* 5: no barcode -> the junction is left to V5 (route never moves). */
    reset_owner(5, 1500); run();
    assert(!has(ROUTE_EV_JUNCTION) && max_phase_seen == ROUTE_IDLE);
    puts("PASS route owner 5: unarmed junction is left to V5");

    /* 7: a letter without a manoeuvre (E) is decoded but not armed. */
    reset_owner(7, 1000); run();
    assert(has(ROUTE_EV_BARCODE) && has(ROUTE_EV_NOT_ARMED) && !has(ROUTE_EV_ARMED) && !route.armed);
    puts("PASS route owner 7: letter E decoded, not armed");

    /* 6: arc never sees the branch -> stops after 6 s with a timeout fault. */
    reset_owner(6, 9000); run();
    assert(has(ROUTE_EV_FAILED) && state.faults == MOTION_FAULT_TIMEOUT && !out_left && !out_right);
    puts("PASS route owner 6: branch never found -> timeout stop, motors off");
}
int main(void)
{
    logic_tests();
    owner_tests();
    return 0;
}
