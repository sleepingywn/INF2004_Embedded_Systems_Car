#include "motion_logic.h"
#include <limits.h>
#include <stddef.h>

void motion_pid_reset(motion_pid_t *pid)
{
    pid->integral_bits = 0;
    pid->error_bits = 0;
    pid->primed = 0;
}

int16_t motion_pid_step(motion_pid_t *pid, int16_t target, int16_t actual, uint16_t dt_ms,
                        uint16_t kp, uint16_t ki, uint16_t kd, uint16_t limit)
{
    int32_t error = (int32_t)target - actual;
    int64_t integral;
    int64_t output;
    int64_t bound = (int64_t)limit * 1024;
    if ((dt_ms == 0u) || (limit > 1000u))
    {
        motion_pid_reset(pid);
        return 0;
    }
    integral = motion_bits_decode(pid->integral_bits, 21u) + (int64_t)ki * error * dt_ms / 1000;
    if (integral > bound)
    {
        integral = bound;
    }
    if (integral < -bound)
    {
        integral = -bound;
    }
    output = (int64_t)kp * error + integral;
    if (pid->primed != 0u)
    {
        output += (int64_t)kd * (error - motion_bits_decode(pid->error_bits, 13u)) * 1000 / dt_ms;
    }
    /* Conditional integration: do not accumulate into output saturation. */
    if (((output <= bound) && (output >= -bound)) || ((output > bound) && (error < 0)) ||
        ((output < -bound) && (error > 0)))
    {
        pid->integral_bits = (uint32_t)motion_bits_encode(integral, 21u);
    }
    if (output > bound)
    {
        output = bound;
    }
    if (output < -bound)
    {
        output = -bound;
    }
    pid->error_bits = (uint16_t)motion_bits_encode(error, 13u);
    pid->primed = 1;
    return (int16_t)(output / 1024);
}

int32_t motion_count_delta(uint32_t current, uint32_t previous)
{
    uint32_t difference = current - previous;
    /* Explicit modular conversion; no signed overflow or unsigned-to-signed
     * out-of-range cast. Requires fewer than 2^31 edges between snapshots. */
    if (difference <= INT32_MAX)
    {
        return (int32_t)difference;
    }
    return (int32_t)((int64_t)difference - INT64_C(4294967296));
}

int8_t motion_quadrature_delta(uint8_t previous, uint8_t current)
{
    /* A is bit 1, B bit 0. 00 -> 10 -> 11 -> 01 is positive.
     * Value 2 denotes an impossible two-bit transition, not two counts. */
    static const int8_t transitions[16] = {0, -1, 1, 2, 1, 0, 2, -1, -1, 2, 0, 1, 2, 1, -1, 0};
    return transitions[((previous & 3u) << 2) | (current & 3u)];
}

bool motion_line_targets(int8_t error, uint16_t cruise, uint16_t maximum, uint16_t gain_q10,
                         int16_t *left, int16_t *right)
{
    if ((left == NULL) || (right == NULL) || (error < -100) || (error > 100) || (cruise == 0u) ||
        (cruise >= maximum) || (maximum > 2047u) || (gain_q10 == 0u))
    {
        return false;
    }
    int32_t correction = (int32_t)((int64_t)error * (int64_t)gain_q10 / 1024);
    int32_t bound = (int32_t)cruise;
    if (bound > (int32_t)maximum - (int32_t)cruise)
    {
        bound = (int32_t)maximum - (int32_t)cruise;
    }
    if (correction > bound)
    {
        correction = bound;
    }
    if (correction < -bound)
    {
        correction = -bound;
    }
    *left = (int16_t)((int32_t)cruise - correction);
    *right = (int16_t)((int32_t)cruise + correction);
    return true;
}

static uint16_t barcode_pattern(const uint8_t *widths)
{
    uint8_t sorted[9];
    uint16_t pattern = 0;
    uint32_t narrow_sum = 0;
    for (unsigned i = 0; i < 9u; ++i)
    {
        if ((widths[i] == 0u) || (widths[i] == 255u))
        {
            return 0;
        }
        sorted[i] = widths[i];
    }
    for (unsigned i = 1; i < 9u; ++i)
    {
        uint8_t value = sorted[i];
        unsigned j = i;
        while ((j > 0u) && (sorted[j - 1u] > value))
        {
            sorted[j] = sorted[j - 1u];
            --j;
        }
        sorted[j] = value;
    }
    /* Code39: three wide, six narrow. Reject ambiguous/noisy clusters.
     * Engineering tolerances (1.5..3.5 ratio) need real-track validation. */
    if ((2u * sorted[6] < 3u * sorted[5]) || (2u * sorted[8] > 7u * sorted[0]) ||
        (sorted[5] > 2u * sorted[0]) || (2u * sorted[8] > 3u * sorted[6]))
    {
        return 0;
    }
    for (unsigned i = 0; i < 6u; ++i)
    {
        narrow_sum += sorted[i];
    }
    if (narrow_sum == 0u)
    {
        return 0;
    }
    for (unsigned i = 0; i < 9u; ++i)
    {
        pattern = (uint16_t)((pattern << 1) | (widths[i] >= sorted[6]));
    }
    return pattern;
}

static char decode_forward(const uint8_t widths[29])
{
    /* Standard Code39 wide-element masks; see README_MOTION.md sources.
     * * = 010010100, A = 100001001, B = 001001001,
     * C = 101001000, D = 000011001. */
    static const uint16_t letters[4] = {0x109u, 0x049u, 0x148u, 0x019u};
    uint16_t middle;
    if ((barcode_pattern(widths) != 0x094u) || (barcode_pattern(widths + 20) != 0x094u))
    {
        return '\0';
    }
    /* Inter-character spaces must be narrow, not another wide element. */
    for (unsigned offset = 0; offset <= 10u; offset += 10u)
    {
        uint8_t minimum = widths[offset];
        for (unsigned i = 1; i < 9u; ++i)
        {
            if (widths[offset + i] < minimum)
            {
                minimum = widths[offset + i];
            }
        }
        if ((widths[offset + 9u] == 0u) || (widths[offset + 9u] > 2u * minimum))
        {
            return '\0';
        }
    }
    middle = barcode_pattern(widths + 10);
    for (unsigned i = 0; i < 4u; ++i)
    {
        if (middle == letters[i])
        {
            return (char)('A' + i);
        }
    }
    return '\0';
}

char motion_barcode_decode(const bc_msg_t *message)
{
    uint8_t reversed[29];
    char command;
    if ((message == NULL) || (message->hdr.count != 29u) || (message->hdr.overflow != 0u) ||
        (message->hdr.reserved != 0u))
    {
        return '\0';
    }
    for (unsigned i = 0; i < 29u; ++i)
    {
        if ((message->widths[i] == 0u) || (message->widths[i] == 255u))
        {
            return '\0';
        }
    }
    command = decode_forward(message->widths);
    if (command != '\0')
    {
        return command;
    }
    for (unsigned i = 0; i < 29u; ++i)
    {
        reversed[i] = message->widths[28u - i];
    }
    return decode_forward(reversed);
}
