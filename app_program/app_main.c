/*
 * Robot firmware startup on the Pico W.
 *
 * Subsystem 1 handles dashboard telemetry and commands, subsystem 2 controls
 * the motors, subsystem 3 reads the line and barcode sensors, and subsystem 4
 * processes IMU measurements.
 *
 * usermain() initializes all subsystems before starting their tasks because
 * a task can run immediately on the second core. After startup it signals
 * motion readiness, starts the console and sleeps to keep the kernel running.
 */

#include "app_main.h"
#include "app.h"
#include "subsystem2/motion.h"
#include "int_console/int_console.h"

/* Startup failure must not proceed into partially initialized consumers.
 * Keep the kernel alive so the output owner can service its stop signal. */
static void startup_check(ER startup_error, const char *name)
{
    if (startup_error < E_OK)
    {
        CHECK(startup_error, name);
        (void)motion_stop();
        for (;;)
        {
            tk_slp_tsk(TMO_FEVR);
        }
    }
}

/*
 * Create and start one task (declared in app.h for every subsystem).
 *   prc = TP_PRC1 (core 0), TP_PRC2 (core 1), or 0 for "any core".
 */
EXPORT ID app_start_task(FP entry, PRI pri, UW prc, const char *name)
{
    T_CTSK ctsk = {0};
    ID tid;
    ER er;

    ctsk.task = entry;            /* function the task runs */
    ctsk.itskpri = pri;           /* initial priority (1 = highest) */
    ctsk.stksz = TASK_STACK_SIZE; /* kernel allocates the stack */
    ctsk.tskatr = TA_HLNG         /* written in a high-level language (C) */
                  | TA_RNG3;      /* runs at user protection level */
#if TK_SUPPORT_SMP
    if (prc != 0)
    {
        ctsk.tskatr |= TA_ASSPRC; /* honour the affinity below */
        ctsk.assprc = prc;        /* which core(s) it may run on */
    }
#else
    (void)prc; /* single-core build: no affinity */
#endif

    /* tk_cre_tsk: create the task.  It starts in the DORMANT state. */
    tid = tk_cre_tsk(&ctsk);
    if (tid < E_OK)
    {
        CHECK(tid, name);
        return tid;
    }

    /* tk_sta_tsk: DORMANT -> READY.  The int is passed as stacd. */
    er = tk_sta_tsk(tid, 0);
    CHECK(er, name);
    if (er < E_OK)
    {
        tk_del_tsk(tid);
        return er;
    }
    return tid;
}

EXPORT INT usermain(void)
{
    T_RVER ver;

    /* tk_ref_ver: ask the kernel who it is */
    tk_ref_ver(&ver);
    tm_printf((UB *)"\n=== micro T-Kernel on Raspberry Pi Pico W"
                    " (spec 0x%04x, product 0x%04x) ===\n",
              ver.spver, ver.prver);
#if TK_SUPPORT_SMP
    tm_printf((UB *)"dual-core (SMP) build, usermain on core %d\n",
              tk_get_prc() - 1); /* tk_get_prc() counts from 1 */
#else
    tm_printf((UB *)"single-core build\n");
#endif

    /* Phase 1: every subsystem creates its kernel objects */
    startup_check(subsystem1_init(), "subsystem1_init");
    startup_check(subsystem2_init(), "subsystem2_init");
    startup_check(subsystem3_init(), "subsystem3_init");
    startup_check(subsystem4_init(), "subsystem4_init");
    startup_check(subsystem5_init(), "subsystem5_init");

    /* Phase 2: every subsystem starts its tasks */
    startup_check(subsystem1_start(), "subsystem1_start");
    startup_check(subsystem2_start(), "subsystem2_start");
    startup_check(subsystem3_start(), "subsystem3_start");
    startup_check(subsystem4_start(), "subsystem4_start");
    startup_check(subsystem5_start(), "subsystem5_start");

    startup_check(motion_application_ready(), "motion_application_ready");
    startup_check(int_console_start(), "int_console_start");
    tm_printf((UB *)"all subsystems started\n\n");

#if APP_TEST
    /* Test build (make TEST=1): run every subsystem's tests in turn,
       here in the initial task, while the system is running.  Tests may
       call tk_dly_tsk() to let the other tasks work for a while. */
    /* Give the USB serial monitor time to connect: output printed before
       the host opens the port is lost (S1/S2 results went missing). */
    tk_dly_tsk(5000);
    tm_printf((UB *)"[TEST] === test run start ===\n");
    subsystem1_test();
    subsystem2_test();
    subsystem3_test();
    subsystem4_test();
    subsystem5_test();
    tm_printf((UB *)"[TEST] === test run end ===\n");
#endif

    /* Sleep forever.  Returning from usermain() would shut down. */
    tk_slp_tsk(TMO_FEVR);
    return 0;
}
