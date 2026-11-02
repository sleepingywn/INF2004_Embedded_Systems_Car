#include "../../app_program/subsystem1/telemetry_wire.h"
#include <stdio.h>
int main(void) {
    s1_wire_values_t v = {0}; uint8_t bytes[S1_FRAME_SIZE];
    v.boot_id=0x12345678; v.sequence=0xffffffff; v.valid=63;
    v.left_speed=-1234; v.right_speed=2047; v.line_error=-100;
    v.pitch_half_deg=-128; v.roll_half_deg=127; v.event_type=19;
    v.event_source=4; v.event_count=42; v.event_value=-7654321;
    s1_encode_frame(bytes,&v);
    for(unsigned i=0;i<S1_FRAME_SIZE;++i) printf("%02x",bytes[i]);
    return 0;
}
