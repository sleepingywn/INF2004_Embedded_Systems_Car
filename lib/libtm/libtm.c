/*
 *----------------------------------------------------------------------
 *    micro T-Kernel 3.00.03
 *
 *    Copyright (C) 2006-2021 by Ken Sakamura.
 *    This software is distributed under the T-License 2.2.
 *----------------------------------------------------------------------
 *
 *    Released by TRON Forum(http://www.tron.org) at 2021/03/31.
 *
 *----------------------------------------------------------------------
 */

/*
 *    libtm.c
 *    T-Monitor compatible calls library
 */
#include <tk/tkernel.h>
#include <tm/tmonitor.h>

#if USE_TMONITOR
#include "libtm.h"

/*
 * libtm_init() - libtm Initialize
 * supported only on wait != 0 (polling not supported)
 */
EXPORT void libtm_init(void)
{
	tm_com_init();
}

/*
 * tm_getchar() - Get Character
 * supported only on wait != 0 (polling not supported)
 */
EXPORT INT tm_getchar( INT wait )
{
	UB	p;
	UINT	imask;

	DI(imask);
	tm_rcv_dat(&p, 1);
	EI(imask);

	return (INT)p;
}

/*
 * tm_getline() - Get Line
 * special key is not supported
 */
EXPORT INT tm_getline( UB *buff )
{
	UB* p = buff;
	int len = 0;
	static const char LF = CHR_LF;
	INT imask;

	DI(imask);
	while (1) {
		tm_rcv_dat(p, 1);
		tm_snd_dat(p, 1); /* echo back */
		if (*p == CHR_CR) {
			tm_snd_dat((const UB*)&LF, 1);
			break;
		} else if (*p == CHR_ETX) {
			len = -1;
			break;
		}
		p++; len++;
	}
	*p = 0x00;
	EI(imask);

	return len;
}

/*
 * tm_putchar()
 * Ctrl-C is not supported
 */
EXPORT INT tm_putchar( INT c )
{
	static const char CR = CHR_CR;
	UB buf = (UB)c;
#if !TM_CONSOLE_USB_CDC
	INT imask;
#endif

#if !TM_CONSOLE_USB_CDC
	DI(imask);
#endif
	if (buf == CHR_LF) {
		tm_snd_dat((const UB*)&CR, 1);
	}
	tm_snd_dat(&buf, 1);
#if !TM_CONSOLE_USB_CDC
	EI(imask);
#endif

	return 0;
}

/*
 * tm_putstring() - Put String
 * Ctrl-C is not supported
 */
EXPORT INT tm_putstring( const UB *buff )
{
	const UB* p = buff;
#if !TM_CONSOLE_USB_CDC
	INT imask;
#endif

#if !TM_CONSOLE_USB_CDC
	DI(imask);
#endif
	while ( *p != (UB)'\0' ) {
		tm_putchar(*p++);
	}
#if !TM_CONSOLE_USB_CDC
	EI(imask);
#endif

	return 0;
}

#endif /* USE_TMONITOR */