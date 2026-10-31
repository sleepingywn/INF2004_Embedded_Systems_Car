#ifndef MOTION_LOGIC_CASES_H
#define MOTION_LOGIC_CASES_H
/* Shared host/on-target pure-logic cases. No peripheral or motor calls. */
#include "motion_logic.h"
#include <string.h>

static bool motion_case_quadrature(void)
{
    const uint8_t clockwise[5] = {0, 2, 3, 1, 0};
    int forward = 0;
    int reverse = 0;
    for (unsigned i = 0; i < 4u; ++i)
    {
        forward += motion_quadrature_delta(clockwise[i], clockwise[i + 1u]);
        reverse += motion_quadrature_delta(clockwise[i + 1u], clockwise[i]);
    }
    return (forward == 4) && (reverse == -4) && (motion_quadrature_delta(0, 3) == 2) &&
           (motion_quadrature_delta(2, 2) == 0);
}

static bool motion_case_wrap(void)
{
    return (motion_count_delta(3u, UINT32_MAX - 1u) == 5) &&
           (motion_count_delta(UINT32_MAX - 1u, 3u) == -5) &&
           (motion_count_delta(100u, 100u) == 0) && (motion_count_delta(99u, 100u) == -1);
}

static bool motion_case_pid(void)
{
    motion_pid_t pid = {0};
    int16_t high = motion_pid_step(&pid, 200, 0, 20, 2048, 1024, 0, 100);
    if ((high != 100) || (pid.integral_bits != 0u))
    {
        return false;
    }
    motion_pid_reset(&pid);
    int16_t reverse = motion_pid_step(&pid, -100, 0, 20, 1024, 0, 0, 200);
    if (reverse != -100)
    {
        return false;
    }
    motion_pid_reset(&pid);
    if (motion_pid_step(&pid, 50, 50, 20, 1024, 0, 0, 200) != 0)
    {
        return false;
    }
    return (motion_pid_step(&pid, 100, 0, 0, 1024, 1024, 1024, 200) == 0) && (pid.primed == 0u);
}

static void motion_case_barcode_fixture(bc_msg_t *barcode, const char *character)
{
    /* Independent run-width fixture: *=nwnnwnwnn, ten samples per narrow,
     * 25 per wide, two ten-sample inter-character spaces. */
    const char *sentinel = "121121211";
    memset(barcode, 0, sizeof(*barcode));
    barcode->hdr.count = 29;
    for (unsigned i = 0; i < 9u; ++i)
    {
        barcode->widths[i] = sentinel[i] == '2' ? 25 : 10;
        barcode->widths[10u + i] = character[i] == '2' ? 25 : 10;
        barcode->widths[20u + i] = barcode->widths[i];
    }
    barcode->widths[9] = barcode->widths[19] = 10;
}

static bool motion_case_barcodes(void)
{
    const char *patterns[4] = {"211112112", "112112112", "212112111", "111122112"};
    bc_msg_t barcode;
    for (unsigned character = 0; character < 4u; ++character)
    {
        motion_case_barcode_fixture(&barcode, patterns[character]);
        if (motion_barcode_decode(&barcode) != (char)('A' + character))
        {
            return false;
        }
        for (unsigned i = 0; i < 14u; ++i)
        {
            uint8_t value = barcode.widths[i];
            barcode.widths[i] = barcode.widths[28u - i];
            barcode.widths[28u - i] = value;
        }
        if (motion_barcode_decode(&barcode) != (char)('A' + character))
        {
            return false;
        }
    }
    motion_case_barcode_fixture(&barcode, patterns[0]);
    barcode.hdr.count = 28;
    if (motion_barcode_decode(&barcode) != 0)
    {
        return false;
    }
    barcode.hdr.count = 29;
    barcode.hdr.overflow = 1;
    if (motion_barcode_decode(&barcode) != 0)
    {
        return false;
    }
    barcode.hdr.overflow = 0;
    barcode.widths[9] = 255;
    if (motion_barcode_decode(&barcode) != 0)
    {
        return false;
    }
    memset(barcode.widths, 10, 29);
    return motion_barcode_decode(&barcode) == 0;
}
static bool motion_case_packing(void)
{
    const int64_t values[7] = {0, -1, 2047, -2047, -8388607, -8388607000LL, 8388607000LL};
    const unsigned widths[7] = {9u, 9u, 12u, 12u, 24u, 34u, 34u};
    for (unsigned i = 0; i < 7u; ++i)
    {
        if (motion_bits_decode(motion_bits_encode(values[i], widths[i]), widths[i]) != values[i])
        {
            return false;
        }
    }
    return (motion_bits_encode(-1, 12u) == UINT64_C(0xfff)) &&
           (motion_bits_decode(UINT64_C(0x800), 12u) == -2048);
}

static bool motion_case_steering(void)
{
    int16_t left = 0;
    int16_t right = 0;
    if (!motion_line_targets(50, 100, 200, 1024, &left, &right) || left != 50 || right != 150)
    {
        return false;
    }
    if (!motion_line_targets(-50, 100, 200, 1024, &left, &right) || left != 150 || right != 50)
    {
        return false;
    }
    if (!motion_line_targets(100, 180, 200, 1024, &left, &right) || left != 160 || right != 200)
    {
        return false;
    }
    return !motion_line_targets(50, 200, 200, 1024, &left, &right);
}
#endif
