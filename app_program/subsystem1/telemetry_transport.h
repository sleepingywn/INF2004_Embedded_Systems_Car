#ifndef TELEMETRY_TRANSPORT_H
#define TELEMETRY_TRANSPORT_H
#include <stdint.h>
#include "telemetry_wire.h"
/* No kernel headers here: safe for inclusion by the lwIP service owner.
 * All operations are nonblocking. 1 = success, 0 = unavailable/contention.
 * Only the Wi-Fi task may peek/ack. Ack only after a successful MQTT QoS 1 completion callback.
 */
int s1_transport_init(void);
int s1_transport_offer(const uint8_t *frame, int event);
int s1_transport_peek(uint8_t *frame, int *event, uint32_t *ticket);
void s1_transport_ack(int event, uint32_t ticket);
uint32_t s1_transport_dropped(void);
#endif
