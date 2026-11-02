/* Compile the production formatter and queue against a deterministic IRQ/lock model.
 * This exercises control flow and data integrity, not real RP2040 latency. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../lib/libtm/sysdepend/pico_rp2040/usb/usb_console.c"
#include "../../lib/libtm/libtm_printf.c"
UINT mock_mask, mock_core, mock_clock;
static unsigned attempts;
static int inject_nested;
UW in_w(UINT address) {return address==SIO_CPUID_ADDR ? mock_core : mock_clock++;}
ER ISpinTryLock(T_SPLOCK *lock,UINT *saved) {
    ++attempts;
    if(inject_nested) {
        inject_nested=0;
        assert(mock_mask==0); /* formatter has not masked interrupts */
        tm_printf((UB*)"IRQ:%u\n",7u);
    }
    *saved=mock_mask;
    mock_mask=1;
    if(*lock) {mock_mask=*saved;return E_BUSY;}
    *lock=1;
    return E_OK;
}
ER ISpinUnlock(T_SPLOCK *lock,UINT saved) {*lock=0;mock_mask=saved;return E_OK;}
#define STUB_INT(name) int name(void){return 0;}
STUB_INT(tm_tusb_init)
STUB_INT(tm_tusb_event_ready)
STUB_INT(tm_tusb_mounted)
STUB_INT(tm_tusb_connected)
int tm_tusb_read_char(void){return -1;}
unsigned tm_tusb_write_available(void){return 0;}
void tm_tusb_service(void){}
void tm_tusb_write_char(char c){(void)c;}
void tm_tusb_write_flush(void){}
void tm_tusb_discard_input(void){}
volatile W tm_usb_service_tskid,tm_usb_def_int_ercd;
volatile UW tm_usb_irq_count,tm_usb_nvic_enabled;
UW tm_tusb_diag_controller_en(void){return 0;}
UW tm_tusb_diag_pullup_en(void){return 0;}
UW tm_tusb_diag_bus_seen(void){return 0;}
static void reset(void) {
    tx_head=tx_tail=0;
#if TK_SUPPORT_SMP
    tx_lock=0;
#endif
    memset((void*)tx_diag,0,sizeof(tx_diag));
    mock_mask=mock_core=attempts=0;
}
static void expect(const char *s) {
    assert(tm_usb_pending()==strlen(s));
    for(unsigned i=0;i<strlen(s);++i) assert(tx_buffer[(tx_tail+i)&USB_TX_MASK]==(UB)s[i]);
}
int main(void) {
    reset();
    assert(tm_printf((UB*)"v=%+d x=%08x s=%s\n",-12,0x123u,"ok")==22);
    expect("v=-12 x=00000123 s=ok\r\n");
    assert(mock_mask==0);
    reset();
    char exact[257];memset(exact,'x',256);exact[256]=0;
    assert(tm_printf((UB*)"%s",exact)==256);
    assert(tx_head==256 && tx_diag[0].truncated_records==0);
    reset();
    assert(tm_printf((UB*)"%s!",exact)==-1);
    assert(tx_head==256 && tx_diag[0].truncated_records==1);
    assert(memcmp((void*)&tx_buffer[251],"...\r\n",5)==0);
    reset();
    /* CRLF expansion also respects the capacity boundary. */
    exact[255]='\n';
    assert(tm_printf((UB*)"%s",exact)==-1);
    assert(tx_head==256 && tx_diag[0].truncated_records==1);
    reset();
    tx_head=4090;tx_tail=4080;
    tm_printf((UB*)"wrapped\n");
    assert(tx_head==3 && tx_buffer[4090]=='w' && tx_buffer[1]=='\r' && tx_buffer[2]=='\n');
    reset();
    tx_head=4093;
    tm_printf((UB*)"ABCD");
    assert(tx_head==4093 && tx_diag[0].dropped_records==1 && tx_diag[0].dropped_bytes==4);
#if TK_SUPPORT_SMP
    reset();tx_lock=1;
    tm_printf((UB*)"busy");
    assert(attempts==1 && tx_head==0 && mock_mask==0);
    assert(tx_diag[0].contentions==1 && tx_diag[0].dropped_bytes==4);
    assert(tm_usb_pending()!=0); /* lock contention must not report a drained queue */
    reset();inject_nested=1;
    tm_printf((UB*)"TASK:%u\n",123u);
    expect("IRQ:7\r\nTASK:123\r\n");
    reset();mock_core=1;tx_lock=1;
    tm_printf((UB*)"core1");
    TM_USB_DIAG d;tm_usb_console_diag(&d);
    assert(d.dropped_records==1 && tx_diag[0].dropped_records==0);
#endif
    reset();mock_mask=1;
    tm_printf((UB*)"masked caller\n");
    assert(mock_mask==1); /* never enable interrupts that the caller disabled */
    reset();
    tm_usb_console_record((UB*)"unused",257,0);
    assert(tx_head==0 && tx_diag[0].dropped_bytes==257);
    puts("Console record boundaries, CRLF, truncation, full/busy queue, reentrancy and IRQ preservation: PASS");
}
