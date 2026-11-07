#ifndef CYW43_LED_REQUEST_H
#define CYW43_LED_REQUEST_H
#include <stdint.h>
#include <stdbool.h>
/* One aligned 32-bit publication: no torn level/timestamp pair across cores.
 * Expire a stopped producer's request instead of leaving a stale solid LED. */
static inline uint32_t cyw43_led_word(uint32_t now,bool on)
{
    return (now<<1) | (on ? 1u : 0u);
}
static inline bool cyw43_led_level(uint32_t word,uint32_t now)
{
    return (word&1u) && (((now&0x7fffffffu)-(word>>1))&0x7fffffffu)<=500u;
}
#endif
