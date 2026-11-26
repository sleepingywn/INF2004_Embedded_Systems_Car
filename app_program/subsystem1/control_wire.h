#ifndef CONTROL_WIRE_H
#define CONTROL_WIRE_H
#include <stdint.h>
#include <stdbool.h>
#include "../subsystem2/route_actions.h"
/* Separate versioned channel: independent of telemetry v1/v2.
 * Command RC/1: op, boot, connection generation, id, issued uptime, epoch.
 * Status RS/1: flags, boot, generation, sequence, uptime, epoch, ack id,
 * ack result, motor state, faults, ack op, remote owner id. Little endian. */
#define CONTROL_COMMAND_SIZE 24u
#define CONTROL_STATUS_SIZE 68u
#define CONTROL_MAPPING_SIZE 54u
#define CONTROL_MAX_AGE_MS 1000u
#define CONTROL_LIVE_MAPPING 32u
#define CONTROL_LEASE_MS 5000u
#define CONTROL_RUN_MS 30000u
#ifndef MOTION_LINE_UNLIMITED
#define MOTION_LINE_UNLIMITED 0
#endif
enum
{
    CONTROL_START = 1,
    CONTROL_STOP = 2,
    CONTROL_KEEP = 3,
    CONTROL_MAPPING = 4
};
enum
{
    CONTROL_NONE,
    CONTROL_QUEUED,
    CONTROL_RUNNING,
    CONTROL_REJECTED,
    CONTROL_STOPPED,
    CONTROL_FINISHED,
    CONTROL_LINK_LOST,
    CONTROL_TIMED_OUT
};
enum
{
    CONTROL_SUPPORTED = 1,
    CONTROL_ARMED = 2,
    CONTROL_ACTIVE = 4,
    CONTROL_MOTION_FRESH = 8,
    CONTROL_CONNECTED = 16
};
typedef struct
{
    uint32_t boot, session, id, issued, epoch;
    uint8_t op;
    uint32_t revision;
    route_actions_t mapping;
} control_command_t;
static inline uint32_t control_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}
static inline void control_put(uint8_t *p, uint32_t n)
{
    for (unsigned i = 0; i < 4; ++i)
    {
        p[i] = (uint8_t)(n >> (8 * i));
    }
}
static inline bool control_decode(const uint8_t *b, unsigned n,
                                  control_command_t *c)
{
    if (n < 4 || b[0] != 'R' || b[1] != 'C')
    {
        return false;
    }
    if (!((n == CONTROL_COMMAND_SIZE && b[2] == 1 && b[3] >= 1 && b[3] <= 3) ||
          (n == CONTROL_MAPPING_SIZE && b[2] == 2 && b[3] == CONTROL_MAPPING)))
    {
        return false;
    }
    *c = (control_command_t){.op = b[3],
                             .boot = control_u32(b + 4),
                             .session = control_u32(b + 8),
                             .id = control_u32(b + 12),
                             .issued = control_u32(b + 16),
                             .epoch = control_u32(b + 20)};
    if (c->op == CONTROL_MAPPING)
    {
        c->revision = control_u32(b + 24);
        for (unsigned i = 0; i < ROUTE_MAP_COUNT; ++i)
        {
            if (!route_action_set(&c->mapping, i, b[28 + i]))
            {
                return false;
            }
        }
    /* RC/2 and RS/2 have fixed byte offsets and lengths. Existing dashboards
     * reject a shorter frame. Expand only at these boundaries; queued
     * commands and the live route table carry three-bit actions. */}
    return c->id != 0;
}
static inline bool control_fresh(const control_command_t *c, uint32_t boot,
                                 uint32_t session, uint32_t epoch, uint32_t now)
{
    return boot && session && c->boot == boot && c->session == session &&
           c->epoch == epoch &&
           (uint32_t)(now - c->issued) <= CONTROL_MAX_AGE_MS;
}
static inline bool control_newer(uint32_t a, uint32_t b)
{
    return (uint32_t)(a - b) != 0 && (uint32_t)(a - b) < 0x80000000u;
}
static inline bool control_expired(uint32_t now, uint32_t renewed,
                                   uint32_t started)
{
    return (uint32_t)(now - renewed) >= CONTROL_LEASE_MS ||
           (!MOTION_LINE_UNLIMITED &&
            (uint32_t)(now - started) >= CONTROL_RUN_MS);
}
#endif
