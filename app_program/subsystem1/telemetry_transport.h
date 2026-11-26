#ifndef TELEMETRY_TRANSPORT_H
#define TELEMETRY_TRANSPORT_H
#include <stdint.h>
#include "telemetry_wire.h"
#include "control_wire.h"
/* No kernel headers here: safe for inclusion by the lwIP service owner.
 * All operations are nonblocking. 1 = success, 0 = unavailable/contention.
 * Only the Wi-Fi task may peek/ack. Ack only after a successful MQTT QoS 1
 * completion callback.
 */
int s1_transport_init(void);
int s1_transport_offer(const uint8_t *frame, int event);
int s1_transport_peek(uint8_t *frame, int *event, uint32_t *ticket);
void s1_transport_ack(int event, uint32_t ticket);
uint32_t s1_transport_dropped(void);
/* The Wi-Fi task owns command ingress and link changes. STOP has a separate
 * latch. */
void s1_control_boot(uint32_t boot);
int s1_control_link(uint32_t session, int connected);
int s1_control_context(uint32_t *boot, uint32_t *session, int *connected);
int s1_control_offer(const uint8_t *frame, unsigned length, uint32_t now_ms);
int s1_control_take(control_command_t *command, uint32_t now, uint32_t last_id,
                    uint32_t epoch, bool motion_fresh);
int s1_control_status_offer(const uint8_t *frame);
int s1_control_status_peek(uint8_t *frame);
#endif
