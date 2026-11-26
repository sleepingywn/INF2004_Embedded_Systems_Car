#define main route_suite_main
#include "line_route_test.c"
#undef main
int main(void) {
 uint8_t a[PAGE],b[PAGE],next[PAGE],map[26],pending[16],decoded[26];
 route_actions_t defaults = active_actions, packed = {0};
 for (unsigned i=0; i<ROUTE_MAP_COUNT; ++i) {
     for (unsigned action=0; action<=4; ++action) {
         assert(route_action_set(&packed,i,action));
         route_mapping_apply(&packed);
         for (unsigned j=0; j<ROUTE_MAP_COUNT; ++j)
             assert(route_mapping_get(j)==(j<i ? 4u : j==i ? action : 0u));
     }
     for (unsigned invalid=5; invalid<=8; ++invalid)
         assert(!route_action_set(&packed,i,invalid) && route_action_at(&packed,i)==4u);
 }
 assert(!route_action_set(&packed,26,0));
 active_actions = defaults;
 route_mapping_export(map);
 assert(route_mapping_code('A')=='A'&&route_mapping_code('Z')=='B'&&!route_mapping_code('E'));
 memset(a,255,PAGE);memset(b,255,PAGE);assert(choose(a,b)==-1);
 make_record(a,map,1);assert(choose(a,b)==0);
 map[0]=4;map[25]=3;make_record(next,map,2);
 /* Power loss at every programmed prefix: never select a partial record. */
 for(unsigned n=0;n<=PAGE;++n){
  memset(b,255,PAGE);memcpy(b,next,n);
  int selected=choose(a,b);assert(selected==0 || (selected==1&&record_valid(b)));
  if(n<40)assert(selected==0);
 }
 assert(choose(a,b)==1);
 for(unsigned bit=0;bit<40*8;++bit){b[bit/8]^=(1u<<(bit%8));assert(choose(a,b)==0);b[bit/8]^=(1u<<(bit%8));}
 make_record(a,map,0xffffffffu);make_record(b,map,0);assert(choose(a,b)==1);
 for(unsigned i=0;i<26;++i)map[i]=i%5;
 pack(pending,map);assert(unpack(pending,decoded)&&!memcmp(map,decoded,26));
 for(unsigned bit=0;bit<128;++bit){pending[bit/8]^=1u<<(bit%8);assert(!unpack(pending,decoded));pending[bit/8]^=1u<<(bit%8);}
 map[25]=5;assert(!route_settings_valid(map));make_record(b,map,1);assert(!record_valid(b));
 /* Actual decoder/route owner: scanned A becomes U-turn, telemetry stays A. */
 reset_owner(1,2000);set_test_mapping('A',4);
 bc_msg_t scan=make_code("WNNNNWNNW",false,100,300);
 route_on_barcode(&scan,400,true);assert(barcode_status.code=='A');assert(route.armed==4u);
 line_msg_t line={.state=LINE_EDGE_ONLY};
 uint16_t levels=line_levels_pack(90,5);
 set_test_mapping('A',2);
 assert(!route_step(420,&line,420,state.epoch,true,levels));
 assert(route.armed==4u && barcode_status.code=='A');
 route_on_barcode(&scan,430,true);
 assert(route.armed==4u && barcode_status.code=='A');
 set_test_mapping('A',0);
 assert(!route_step(440,&line,440,state.epoch,true,levels));
 assert(route.armed==4u);
 route_reset();
 route_on_barcode(&scan,500,true);assert(!route.armed&&barcode_status.code=='A');
 set_test_mapping('A',2);
 route_on_barcode(&scan,520,true);
 line.state=LINE_BOTH_BLACK;levels=line_levels_pack(90,90);
 line_trial.adaptive.mode=LINE_ADAPT_CROSSING;
 assert(!route_step(540,&line,540,state.epoch,true,levels));
 set_test_mapping('A',4);
 assert(route_step(560,&line,560,state.epoch,true,levels));
 assert(!route.armed && route.phase==ROUTE_ARC_LEFT && route.doing=='B');
 set_test_mapping('A',4);
 state.raw_valid=1; /* encoder snapshot for the running arc */
 route_step(580,&line,580,state.epoch,true,levels);
 assert(route.doing=='B' && route.phase==ROUTE_ARC_LEFT);
 puts("PASS mapping journal: blank/corrupt flash, every interrupted program prefix, CRC bit flips, revision wrap, pending CRC, remapped/ignored actual barcode");
}
