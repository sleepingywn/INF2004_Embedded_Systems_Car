#ifndef CONSOLE_TEST_KERNEL_H
#define CONSOLE_TEST_KERNEL_H
#include <stdint.h>
#include <stddef.h>
#ifndef TK_SUPPORT_SMP
#define TK_SUPPORT_SMP 1
#endif
#define TK_MAX_CORE (TK_SUPPORT_SMP ? 2 : 1)
#define USE_TMONITOR 1
#define USE_TM_PRINTF (1)
#define TM_CONSOLE_USB_CDC 1
#define TM_CONSOLE_UART 0
#define TM_OUTBUF_SZ (0)
#define LOCAL static
#define EXPORT
#define IMPORT extern
#define TRUE 1
#define FALSE 0
#define E_OK 0
#define E_BUSY (-1)
#define TA_HLNG 1
#define TA_RNG0 2
#define TA_ASSPRC 4
#define TP_PRC1 1
#define TIMER_TIMERAWL 1
#define SIO_CPUID_ADDR 2
#define DI(s) ((s)=mock_mask,mock_mask=1)
#define EI(s) (mock_mask=(s))
typedef uint8_t UB;
typedef int16_t H;
typedef uint16_t UH;
typedef uint32_t UW, UINT;
typedef int32_t W, INT, ER, ID, BOOL;
typedef void (*FP)(void);
typedef volatile UW T_SPLOCK;
typedef struct {void *exinf; UINT tskatr,assprc; FP task; INT itskpri,stksz; void *bufptr;} T_CTSK;
extern UINT mock_mask, mock_core, mock_clock;
UW in_w(UINT address);
ER ISpinTryLock(T_SPLOCK *,UINT *);
ER ISpinUnlock(T_SPLOCK *,UINT);
static inline ID tk_get_tid(void) {return 1;}
static inline void tk_ext_tsk(void) {}
static inline void tk_slp_tsk(INT ms) {(void)ms;}
static inline ID tk_cre_tsk(T_CTSK *c) {(void)c;return 1;}
static inline ER tk_sta_tsk(ID t,INT s) {(void)t;(void)s;return E_OK;}
static inline ER tk_del_tsk(ID t) {(void)t;return E_OK;}
INT tm_putchar(INT);
INT tm_putstring(const UB *);
INT tm_printf(const UB *,...);
INT tm_sprintf(UB *,const UB *,...);
#endif
