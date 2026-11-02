#ifndef MOTION_HOST_SHIM_H
#define MOTION_HOST_SHIM_H
#include <stdint.h>
#include <stdbool.h>
#define APP_H
#define TM_USB_CONSOLE_H
#define TM_CONSOLE_USB_CDC 1
#define TM_WIFI_CYW43 0
#define TK_SUPPORT_SMP 0
#ifndef MOTION_LINE_DEMO
#define MOTION_LINE_DEMO 1
#endif
typedef int ER,ID,INT; typedef unsigned UINT; typedef uint32_t UW;typedef void (*FP)(void);
typedef struct {unsigned mtxatr;} T_CMTX;
typedef struct {unsigned unused;} T_CFLG;
typedef struct {unsigned cycatr;FP cychdr;unsigned cyctim,cycphs;} T_CCYC;
typedef struct {UINT flgptn;} T_RFLG;
#define E_OK 0
#define E_PAR (-17)
#define E_OBJ (-41)
#define E_TMOUT (-50)
#define E_NOEXS (-52)
#define TMO_POL 0
#define TA_INHERIT 1
#define TA_HLNG 1
#define TP_PRC2 2
#define TWF_ORW 1
#define TWF_BITCLR 2
#define TIMER_TIMERAWH 1
#define TIMER_TIMERAWL 2
#define DI(x) ((x)=0)
#define EI(x) ((void)(x))
ER tk_loc_mtx(ID,int);ER tk_unl_mtx(ID);ER tk_set_flg(ID,UINT);ER tk_clr_flg(ID,UINT);
ER tk_ref_flg(ID,T_RFLG*);ER tk_wai_flg(ID,UINT,UINT,UINT*,int);
ID tk_cre_mtx(const T_CMTX*);ID tk_cre_flg(const T_CFLG*);ID tk_cre_cyc(const T_CCYC*);
ID app_start_task(FP,int,int,const char*);ER tk_sta_cyc(ID);
UW in_w(UW);UW tm_usb_state(void);
#endif
