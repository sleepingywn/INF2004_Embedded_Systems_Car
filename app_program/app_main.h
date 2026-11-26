#ifndef APP_APP_MAIN_H
#define APP_APP_MAIN_H

#include <tk/tkernel.h>

IMPORT ID app_start_task(FP entry, PRI pri, UW prc, const char *name);
IMPORT INT usermain(void);

#endif
