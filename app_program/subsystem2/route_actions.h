#ifndef ROUTE_ACTIONS_H
#define ROUTE_ACTIONS_H

#include <stdbool.h>
#include <stdint.h>

#define ROUTE_MAP_COUNT 26u
#define ROUTE_ACTION_BITS 3u
#define ROUTE_ACTIONS_PER_WORD 10u
#define ROUTE_ACTION_WORDS 3u
#define ROUTE_ACTION_MAX 4u

typedef struct {
    uint32_t words[ROUTE_ACTION_WORDS];
} route_actions_t;
/* A wheel-control lookup must see one complete action during a dashboard edit.
 * Ten three-bit actions fit per aligned word without straddling an atomic load. */

static inline unsigned route_action_shift(unsigned index)
{
    return (index % ROUTE_ACTIONS_PER_WORD) * ROUTE_ACTION_BITS;
}

static inline uint8_t route_action_at(const route_actions_t *actions, unsigned index)
{
    uint32_t word = actions->words[index / ROUTE_ACTIONS_PER_WORD];
    return (uint8_t)((word >> route_action_shift(index)) & 7u);
}

static inline bool route_action_set(route_actions_t *actions, unsigned index, unsigned value)
{
    if (index >= ROUTE_MAP_COUNT || value > ROUTE_ACTION_MAX) return false;
    unsigned shift = route_action_shift(index);
    uint32_t *word = &actions->words[index / ROUTE_ACTIONS_PER_WORD];
    *word = (*word & ~(7u << shift)) | ((uint32_t)value << shift);
    return true;
}

static inline bool route_actions_valid(const route_actions_t *actions)
{
    for (unsigned i = 0; i < ROUTE_MAP_COUNT; ++i) {
        if (route_action_at(actions, i) > ROUTE_ACTION_MAX) return false;
    }
    return true;
}

#endif
