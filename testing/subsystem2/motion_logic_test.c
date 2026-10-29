/* Host-only runner. Not built by the firmware makefile. NOT RUN during authoring. */
#include "../../app_program/subsystem2/motion_logic_cases.h"
#include <stdio.h>

int main(void)
{
    const bool results[6] = {motion_case_quadrature(), motion_case_wrap(),
                             motion_case_pid(),        motion_case_barcodes(),
                             motion_case_packing(),    motion_case_steering()};
    const unsigned identifiers[6] = {3u, 4u, 5u, 6u, 17u, 18u};
    int failures = 0;
    for (unsigned i = 0; i < 6u; ++i)
    {
        printf("[TEST] S2-U%02u %s\n", identifiers[i], results[i] ? "PASS" : "FAIL");
        if (!results[i])
        {
            ++failures;
        }
    }
    return failures != 0;
}
