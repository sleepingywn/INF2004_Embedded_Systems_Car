#ifndef TM_USB_CONSOLE_H
#define TM_USB_CONSOLE_H

#include <tk/typedef.h>

#define TM_USB_RECORD_SIZE 256u
typedef struct {
    UW dropped_bytes;
    UW dropped_records;
    UW truncated_records;
    UW contentions;
    UW max_enqueue_us; /* attempt + copy/accounting, excluding final unlock */
} TM_USB_DIAG;

/* One bounded record, accepted whole or dropped; safe before task startup.
 * No waiting for USB or a busy software lock. Newline conversion is caller's. */
IMPORT void tm_usb_console_record(const UB *buf, INT size, INT truncated);
/* Approximate sum/max of independently updated per-core counters. */
IMPORT void tm_usb_console_diag(TM_USB_DIAG *diag);
/* Existing USB service state: 3 means host asserted DTR. */
IMPORT UW tm_usb_state(void);
/* RP2040: caller must stop outputs first. Success reboots and never returns. */
IMPORT INT tm_usb_enter_bootsel(void);
#endif
