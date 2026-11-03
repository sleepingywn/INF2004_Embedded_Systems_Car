#include "subsystem1.h"
#include "diagnostic_keys.h"
#include "../subsystem2/motion.h"
#include "../subsystem2/line_console.h"
#include "../subsystem3/ir_runtime.h"
#if TM_CONSOLE_USB_CDC
#include <tm/usb_console.h>
#if TK_SUPPORT_SMP
#include "../../kernel/knlinc/smp_lock.h"
#endif
IMPORT INT tm_usb_console_getc(void);
static void print_diagnostics(void)
{
    motion_line_console_status();
    motion_diagnostics_t d;
    ir_runtime_status_t ir;
    if (motion_get_diagnostics(&d) == E_OK) {
        tm_printf((UB*)"[diag] sources=0x%x timeout_reason=%u first_ms=%u interval_ms=%u state=%u max_loop_ms=%u app_ms=%u radio_ms=%u released_ms=%u\n",
            (UINT)d.fault_sources, (UINT)d.timeout_reason, (UINT)d.first_timeout_ms,
            (UINT)d.first_timeout_interval_ms, (UINT)d.first_timeout_state,
            (UINT)d.max_loop_ms, (UINT)d.app_ready_ms, (UINT)d.radio_init_ms, (UINT)d.released_ms);
    } else tm_printf((UB*)"[diag] motion snapshot busy\n");
    if (ir_runtime_get_status(&ir) == E_OK) {
        tm_printf((UB*)"[diag] IR state=%u running=%u faults=0x%x samples=%u raw=%u,%u,%u raw_ms=%u\n",
            (UINT)ir.state,(UINT)ir.acquisition.running,(UINT)ir.acquisition.faults,
            (UINT)ir.acquisition.samples,(UINT)ir.raw.barcode,(UINT)ir.raw.line_a,
            (UINT)ir.raw.line_b,(UINT)ir.raw.sampled_ms);
        tm_printf((UB*)"[diag] IR calibration_samples=%u failed_channels=0x%x handoff_errors=%u lateness_us=%u scan_us=%u\n",
            (UINT)ir.calibration_samples,(UINT)ir.failed_channels,(UINT)ir.handoff_errors,
            (UINT)ir.acquisition.max_lateness_us,(UINT)ir.acquisition.max_scan_us);
        tm_printf((UB*)"[diag] IR handoff_stage=%u handoff_error=%d handoff_ms=%u epoch_resyncs=%u\n",
            (UINT)ir.last_handoff_stage,(INT)ir.last_handoff_error,(UINT)ir.last_handoff_ms,(UINT)ir.epoch_resyncs);
        tm_printf((UB*)"[diag] IR first_reason=%u first_us=%u detail=%u first_samples=%u\n",
            (UINT)ir.acquisition.first_fault_reason,(UINT)ir.acquisition.first_fault_us,
            (UINT)ir.acquisition.first_fault_detail,(UINT)ir.acquisition.first_fault_samples);
    } else tm_printf((UB*)"[diag] IR snapshot busy\n");
    TM_USB_DIAG usb;
    tm_usb_console_diag(&usb);
    tm_printf((UB*)"[diag] USB enqueue_max_us=%u busy=%u dropped_records=%u dropped_bytes=%u truncated=%u\n",
        (UINT)usb.max_enqueue_us,(UINT)usb.contentions,(UINT)usb.dropped_records,
        (UINT)usb.dropped_bytes,(UINT)usb.truncated_records);
#if TK_SUPPORT_SMP
    KNL_LOCK_DIAG kernel;
    knl_kernel_lock_audit(&kernel);
    tm_printf((UB*)"[diag] kernel hold_max_us=%u wait_max_us=%u contentions=%u\n",
        (UINT)kernel.max_hold_us,(UINT)kernel.max_wait_us,(UINT)kernel.contentions);
#endif
}
static void diagnostic_task(INT stacd, void *exinf)
{
    (void)stacd; (void)exinf;
    tk_dly_tsk(3000);
#if MOTION_LINE_DEMO
    tm_printf((UB*)"[diag] h help, i diagnostics, c begin IR calibration, f finish. Local bounded line profile enabled.\n");
    motion_line_console_help();
#else
    tm_printf((UB*)"[diag] h help, i diagnostics, c begin IR calibration, f finish. Motor/reset/bypass commands disabled.\n");
#endif
    print_diagnostics();
    for (;;) {
        /* Bounded input processing, at most one character per 100 ms. */
        INT c = tm_usb_console_getc();
        if(motion_line_console_key(c)) {tk_dly_tsk(100);continue;}
        switch (s1_diagnostic_key(c)) {
        case S1_DIAG_HELP:
            motion_line_console_help();
            tm_printf((UB*)"[diag] i status; c then manually expose ALL IR sensors to white/black; f finish; i verify. No automatic calibration or motion.\n"); break;
        case S1_DIAG_STATUS: print_diagnostics(); break;
        case S1_DIAG_BEGIN:
            tm_printf((UB*)"[diag] IR BEGIN accepted=%d\n",(INT)ir_runtime_calibration(IR_RUNTIME_BEGIN)); break;
        case S1_DIAG_FINISH:
            tm_printf((UB*)"[diag] IR FINISH accepted=%d; use i to inspect completion\n",(INT)ir_runtime_calibration(IR_RUNTIME_FINISH)); break;
        default: break;
        }
        tk_dly_tsk(100);
    }
}
ER s1_diagnostic_start(void)
{
    ID id = app_start_task((FP)diagnostic_task,22,TP_PRC1,"s1_diagnostics");
    return id < E_OK ? id : E_OK;
}
#else
ER s1_diagnostic_start(void) { return E_OK; }
#endif
