#ifndef LINE_ADAPTIVE_CONFIG_H
#define LINE_ADAPTIVE_CONFIG_H
#include <stdint.h>
/* Count-domain calibration, independent of course order/curve duration.
 * Provisional until tested on multiple unseen layouts with unchanged settings.
 * No encoder count here is claimed to represent a calibrated distance/angle. */
typedef struct
{
    int32_t cruise, minimum_rolling, maximum, search_speed;
    int32_t kp_milli, kd_milli, max_steering;
    uint32_t filter_ms, derivative_ms;
    unsigned seen_enter, seen_leave, black;
    uint32_t confirm_ms, pause_confirm_ms, uncertainty_counts, bend_hold_counts;
    uint32_t first_sweep_counts, opposite_sweep_counts, recovery_counts,
        recovery_ms;
} line_adaptive_config_t;
extern const line_adaptive_config_t line_adaptive_config;
#endif
