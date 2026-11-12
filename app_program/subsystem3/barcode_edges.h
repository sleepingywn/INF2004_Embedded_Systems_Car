#ifndef BARCODE_EDGES_H
#define BARCODE_EDGES_H
#include <stdbool.h>
#include <stdint.h>
#include "ir_types.h"

/* One producer (shared GPIO ISR), one consumer (S3). No kernel calls in ISR. */
#define BC_EDGE_RING 128u
#define BC_GLITCH_US 2000u
#define BC_EDGE_QUIET_US 1500000u
#define BC_EDGE_STALL_US 3000000u
/* Sum of both wheel counts; configured by the board profile. */
#ifndef BC_QUIET_COUNTS
#define BC_QUIET_COUNTS 80u
#endif
typedef struct { uint32_t us, position, invalid; bool black; } bc_edge_t;
typedef struct {
    bc_msg_t message;
    bc_edge_t stable, candidate;
    uint32_t dropped, glitches, last_edge_us, last_move_us, last_position;
    bool primed, pending, active, distance_ok;
} bc_edges_t;
void bc_edge_irq(uint32_t us, uint32_t position, uint32_t invalid, bool black, bool ambiguous);
void bc_edge_position(uint32_t position);
void bc_edges_reset(bc_edges_t *c);
bool bc_edges_poll(bc_edges_t *c, uint32_t now_us, bool black, bc_msg_t *out);
#endif
