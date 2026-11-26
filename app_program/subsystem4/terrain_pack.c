/* Pack validated terrain measurements for the shared eight-byte snapshot. */
#include "terrain_pack.h"
#include <math.h>

/* Inputs are finite. Clamp before scaling so even FLT_MAX stays in range. */
static int32_t quantize(float value, float scale, int32_t low, int32_t high)
{
    if (value <= ((float)low / scale))
    {
        return low;
    }
    if (value >= ((float)high / scale))
    {
        return high;
    }

    float scaled = value * scale;
    /* Field limits bound the rounded result before integer conversion. */
    return (int32_t)((scaled >= 0.0f) ? (scaled + 0.5f) : (scaled - 0.5f));
}

S4_TERRAIN terrain_pack(const terrain_values_t *p_values)
{
    S4_TERRAIN result = {0};
    bool angles_ok =
        isfinite(p_values->pitch_deg) && isfinite(p_values->roll_deg);
    bool peaks_ok =
        isfinite(p_values->cur_peak_mm) && isfinite(p_values->high_peak_mm);

    /* The shared validity flag covers attitude and peaks together. */
    if (angles_ok && peaks_ok)
    {
        /* Conversion to uint8_t encodes negative results modulo 256. */
        result.pitch_hd_bits =
            (uint8_t)quantize(p_values->pitch_deg, 2.0f, -128, 127);
        result.roll_hd_bits =
            (uint8_t)quantize(p_values->roll_deg, 2.0f, -128, 127);
        result.cur_peak_mm =
            (uint64_t)quantize(p_values->cur_peak_mm, 1.0f, 0, 127);
        result.high_peak_mm =
            (uint64_t)quantize(p_values->high_peak_mm, 1.0f, 0, 127);
    }

    /* Turn rate has a separate unavailable code outside its valid range. */
    result.turn_2dps_bits = (uint8_t)S4_TURN_NA;
    if (p_values->turn_valid && isfinite(p_values->turn_dps))
    {
        result.turn_2dps_bits =
            (uint8_t)quantize(p_values->turn_dps, 0.5f, -127, 127);
    }

    result.hump_state =
        (p_values->hump_state <= 2u) ? p_values->hump_state : 0u;
    result.motion_class = p_values->motion_class & 0x7u;
    result.hump_count = p_values->hump_count & 0xFu;
    result.calibrated = p_values->calibrated;
    result.imu_fault = p_values->imu_fault || !angles_ok || !peaks_ok;
    result.time_ms = p_values->time_ms & S4_TIME_MASK;
    return result;
}
