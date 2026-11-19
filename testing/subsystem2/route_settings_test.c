#define main route_suite_main
#include "line_route_test.c"
#undef main
int main(void) {
 uint8_t a[PAGE],b[PAGE],next[PAGE],map[26],pending[16],decoded[26];
 memcpy(map,route_mapping,26);
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
 reset_owner(1,2000);route_mapping[0]=4;
 bc_msg_t scan=make_code("WNNNNWNNW",false,100,300);
 route_on_barcode(&scan,400,true);assert(barcode_status.code=='A');assert(route.armed=='D');
 route_mapping[0]=2;assert(route.armed=='D'); /* earlier decision survives the edit */
 route_on_barcode(&scan,450,true);assert(route.armed=='B'); /* next A uses left */
 route.phase=ROUTE_IDLE;route.armed=0;route_mapping[0]=0;
 route_on_barcode(&scan,500,true);assert(!route.armed&&barcode_status.code=='A');
 puts("PASS mapping journal: blank/corrupt flash, every interrupted program prefix, CRC bit flips, revision wrap, pending CRC, remapped/ignored actual barcode");
}
