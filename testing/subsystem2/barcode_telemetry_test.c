#define main route_suite_main
#include "line_route_test.c"
#undef main
int main(void) {
 reset_owner(1,2000);barcode_status=(motion_barcode_status_t){0};publish_observer_status();
 motion_barcode_status_t b;assert(motion_get_barcode_status(&b)==E_OK&&!b.seen);
 run();assert(motion_get_barcode_status(&b)==E_OK&&b.seen&&b.code=='A'&&b.sampled_ms==400&&b.elements==29);
 uint32_t seq=b.sequence,reader=0;motion_route_barcode_t console;
 (void)motion_route_barcode(&console,&reader);drain_events();
 assert(motion_get_barcode_status(&b)==E_OK&&b.sequence==seq&&b.code=='A');
 bc_msg_t failed={0};failed.hdr.count=9;
 route_on_barcode(&failed,clock_ms,false);publish_observer_status();
 assert(motion_get_barcode_status(&b)==E_OK&&b.seen&&!b.code&&b.sequence==seq+1&&b.elements==9);
 failed.hdr.count=3;route_on_barcode(&failed,clock_ms,false);publish_observer_status();
 assert(motion_get_barcode_status(&b)==E_OK&&b.sequence==seq+1); /* blips ignored */
 bc_msg_t z=make_code("NWWNWNNNN",false,100,300);
 route_log_head=route_log_tail+ROUTE_LOG_SIZE; /* telemetry cannot depend on FIFO space */
 route_on_barcode(&z,clock_ms,false);publish_observer_status();
 assert(motion_get_barcode_status(&b)==E_OK&&b.code=='Z'&&b.sequence==seq+2);
 failed.hdr.count=5;route_on_barcode(&failed,clock_ms,false);publish_observer_status();
 assert(motion_get_barcode_status(&b)==E_OK&&b.elements==5&&!b.code&&b.sequence==seq+3);
 puts("PASS barcode snapshots: actual A route, independent console reader, failed scan, ignored blip, Z with full console FIFO");
}
