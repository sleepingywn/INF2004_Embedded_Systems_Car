#include "../../app_program/subsystem1/telemetry_time.h"
#include "../../app_program/subsystem1/diagnostic_keys.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
 /* Recorded failure: hardware uptime about 1160 ms ahead of kernel clock. */
 assert(s1_terrain_age(10000,9990)==10);
 assert(s1_terrain_fresh(10000,9990,11160,11160,100));
 assert(!s1_terrain_fresh(10101,9990,11261,11261,100));
 assert(s1_terrain_fresh(32775,32760,34000,33990,100));
 /* Old packed timestamps must not resurrect after a 32.768-second wrap. */
 assert(!s1_terrain_fresh(42768,9990,43928,11160,100));
 assert(s1_terrain_fresh(10000,9990,20,UINT32_MAX-29,100));
 for(int c=-1;c<256;++c) {
  s1_diag_key_t k=s1_diagnostic_key(c);
  if(c=='h'||c=='?')assert(k==S1_DIAG_HELP);
  else if(c=='i')assert(k==S1_DIAG_STATUS);
  else if(c=='c')assert(k==S1_DIAG_BEGIN);
  else if(c=='f')assert(k==S1_DIAG_FINISH);
  else assert(k==S1_DIAG_IGNORE);
 }
 puts("Clock offset/wrap/stale watchdog and diagnostic command allowlist: PASS");return 0;
}
