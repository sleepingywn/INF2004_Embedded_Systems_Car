#ifndef ROUTE_SETTINGS_H
#define ROUTE_SETTINGS_H
#include <stdint.h>
#include <stdbool.h>
#include "route_actions.h"
/* Wire/storage actions: ignore, right, left, straight 100 mm, U-turn. */
void route_mapping_apply(const route_actions_t *actions);
uint8_t route_mapping_get(unsigned index);
void route_mapping_export(uint8_t map[ROUTE_MAP_COUNT]);
extern uint32_t route_mapping_revision;
extern bool route_mapping_persisted;
void route_settings_boot(void);
void route_settings_reboot(const uint8_t map[ROUTE_MAP_COUNT]);
bool route_settings_valid(const uint8_t *map);
static inline char route_mapping_code(char scanned)
{
    if (scanned < 'A' || scanned > 'Z')
    {
        return 0;
    }
    unsigned action = route_mapping_get((unsigned)(scanned - 'A'));
    return action && action <= 4 ? (char)('A' + action - 1) : 0;
}
#endif
