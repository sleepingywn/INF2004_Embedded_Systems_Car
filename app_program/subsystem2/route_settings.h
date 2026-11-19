#ifndef ROUTE_SETTINGS_H
#define ROUTE_SETTINGS_H
#include <stdint.h>
#include <stdbool.h>
#define ROUTE_MAP_COUNT 26u
/* Wire/storage actions: ignore, right, left, straight 100 mm, U-turn. */
extern uint8_t route_mapping[ROUTE_MAP_COUNT];
extern uint32_t route_mapping_revision;
extern bool route_mapping_persisted;
void route_settings_boot(void);
void route_settings_reboot(const uint8_t map[ROUTE_MAP_COUNT]);
bool route_settings_valid(const uint8_t *map);
static inline char route_mapping_code(char scanned) {
    if(scanned<'A' || scanned>'Z')return 0;
    unsigned action=__atomic_load_n(&route_mapping[(unsigned)(scanned-'A')],__ATOMIC_RELAXED);
    return action && action<=4 ? (char)('A'+action-1) : 0;
}
#endif
