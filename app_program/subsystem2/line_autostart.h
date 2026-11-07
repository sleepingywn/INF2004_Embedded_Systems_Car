#ifndef MOTION_LINE_AUTOSTART_H
#define MOTION_LINE_AUTOSTART_H
#include <stdint.h>
#include <stdbool.h>
#define LINE_AUTO_PREPARE_MS 4000u
#define LINE_AUTO_START_MS 5000u
#define LINE_AUTO_LATEST_MS 5500u
typedef enum { LINE_AUTO_WAIT, LINE_AUTO_RESET_PENDING, LINE_AUTO_CONSUMED,
               LINE_AUTO_CANCELLED } line_auto_phase_t;
typedef enum { LINE_AUTO_NONE, LINE_AUTO_RESET, LINE_AUTO_TRACE,
               LINE_AUTO_ABORT } line_auto_action_t;
typedef struct { line_auto_phase_t phase; } line_auto_t;
static inline line_auto_action_t line_auto_step(line_auto_t *a,uint32_t boot_ms,
                                               bool reset_ready,bool trace_ready)
{
    if(a->phase==LINE_AUTO_CANCELLED || a->phase==LINE_AUTO_CONSUMED)return LINE_AUTO_NONE;
    /* Do not start unexpectedly if task scheduling/startup missed the window. */
    if(boot_ms>LINE_AUTO_LATEST_MS) {a->phase=LINE_AUTO_CANCELLED;return LINE_AUTO_ABORT;}
    if(a->phase==LINE_AUTO_WAIT) {
        if(boot_ms<LINE_AUTO_PREPARE_MS)return LINE_AUTO_NONE;
        if(boot_ms>=LINE_AUTO_START_MS || !reset_ready) {
            a->phase=LINE_AUTO_CANCELLED;return LINE_AUTO_ABORT;
        }
        a->phase=LINE_AUTO_RESET_PENDING;return LINE_AUTO_RESET;
    }
    if(boot_ms<LINE_AUTO_START_MS)return LINE_AUTO_NONE;
    a->phase=trace_ready?LINE_AUTO_CONSUMED:LINE_AUTO_CANCELLED;
    return trace_ready?LINE_AUTO_TRACE:LINE_AUTO_ABORT;
}
#endif
