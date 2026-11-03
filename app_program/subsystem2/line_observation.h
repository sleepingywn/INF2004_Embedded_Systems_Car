#ifndef MOTION_LINE_OBSERVATION_H
#define MOTION_LINE_OBSERVATION_H
#include <stdint.h>
#include <stdbool.h>
/* Two 0..100 calibrated levels, 7 bits each, plus validity. Stored with the
 * existing line message under its same timestamp, epoch and mailbox lock. */
static inline uint16_t line_levels_pack(unsigned left,unsigned right)
{
    return left<=100 && right<=100 ? (uint16_t)(0x8000u|left|(right<<7)):0;
}
static inline bool line_levels_valid(uint16_t bits)
{
    return (bits&0xc000u)==0x8000u && (bits&127u)<=100 && ((bits>>7)&127u)<=100;
}
static inline unsigned line_level_left(uint16_t bits){return bits&127u;}
static inline unsigned line_level_right(uint16_t bits){return (bits>>7)&127u;}
#endif
