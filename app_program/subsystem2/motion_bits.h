#ifndef MOTION_BITS_H
#define MOTION_BITS_H
#include <stdint.h>

/* Unsigned storage with explicit two's-complement encoding, BARR-C 5.3a.
 * Internal callers use constant widths 9..34 and check ranges BEFORE encoding.
 * Never shift a signed operand, nor rely on signed bit-field representation. */
static inline uint64_t motion_bits_encode(int64_t value, unsigned width)
{
    return (uint64_t)value & ((UINT64_C(1) << width) - UINT64_C(1));
}

static inline int64_t motion_bits_decode(uint64_t value, unsigned width)
{
    uint64_t modulus = UINT64_C(1) << width;
    value &= modulus - UINT64_C(1);
    return (value < (modulus / UINT64_C(2))) ? (int64_t)value : (int64_t)value - (int64_t)modulus;
}
#endif
