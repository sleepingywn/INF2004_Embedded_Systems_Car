#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <tk/tkernel.h>
typedef INT PRI;
typedef struct {UINT intatr;FP inthdr;} T_DINT;
#define E_SYS (-5)
#define E_PAR (-6)
#define TIMER_TIMERAWH 3
#define TIMER_ALARM3 4
#define TIMER_INTE 5
#define TIMER_ARMED 6
#define TIMER_INTR 7
#define TIMER_INTF 8
#define RESETS_RESET 9
#define RESETS_RESET_DONE 10
#define RESETS_RESET_ADC 1u
#define RESETS_RESET_TIMER 2u
#define GPIO_CTRL(pin) (100+(pin))
#define GPIO(pin) (200+(pin))
#define GPIO_CTRL_FUNCSEL_NULL 31u
#define GPIO_IE 1u
#define GPIO_PUE 2u
#define GPIO_PDE 4u
static unsigned wakes;
static void DisableInt(UINT irq){(void)irq;}
static void EnableInt(UINT irq,UINT pri){(void)irq;(void)pri;}
static void ClearInt(UINT irq){(void)irq;}
static ER tk_set_flg(ID event,UINT flags){(void)event;(void)flags;++wakes;return E_OK;}
static ER tk_def_int(UINT irq,T_DINT *def){(void)irq;(void)def;return E_OK;}
static void out_w(UINT address,UW value);
static void set_w(UINT address,UW value){out_w(address,value);}
static void clr_w(UINT address,UW value){out_w(address,value);}
#include "../../app_program/subsystem3/ir_acquisition.c"
UINT mock_mask,mock_core,mock_clock;
static UW fifo_value,fifo_status,last_adc_command;
UW in_w(UINT address) {
    if(address==TIMER_TIMERAWL) return mock_clock;
    if(address==TIMER_TIMERAWH) return 0;
    if(address==ADC_CS) return ADC_CS_READY;
    if(address==ADC_FCS) return fifo_status;
    if(address==ADC_FIFO) return fifo_value;
    if(address==RESETS_RESET_DONE) return RESETS_RESET_ADC|RESETS_RESET_TIMER;
    return 0;
}
static void out_w(UINT address,UW value){if(address==ADC_CS)last_adc_command=value;}
static void reset(void) {
    acquisition=(ir_acq_status_t){0};
    memset(&scan,0,sizeof(scan));
    queue.head=queue.tail=0;
    acquisition.running=true;acquisition.samples=12;
    scan.deadline_us=10000;scan.deadline_ms=10;
    mock_clock=10000;fifo_status=IR_FIFO_EMPTY;fifo_value=0;wakes=0;
}
int main(void) {
    /* Vary only ADC2 through the real scan ISR. Verify channel selection and
     * unscaled raw propagation independently of white/black calibration. */
    const unsigned right_values[]={43,160,2533};
    for(unsigned i=0;i<3;++i) {
        reset();timer_interrupt(3);
        assert(scan.active && scan.channel==IR_CH_BARCODE);
        assert(last_adc_command==(ADC_CS_EN|ADC_CS_STRAT_ONCE));
        fifo_status=0;fifo_value=183;adc_interrupt(22);
        assert(scan.channel==IR_CH_LINE_A);
        assert(last_adc_command==(ADC_CS_EN|ADC_CS_STRAT_ONCE|(1u<<ADC_CS_AINSEL_POS)));
        fifo_value=173;adc_interrupt(22);
        assert(scan.channel==IR_CH_LINE_B);
        assert(last_adc_command==(ADC_CS_EN|ADC_CS_STRAT_ONCE|(2u<<ADC_CS_AINSEL_POS)));
        fifo_value=right_values[i];adc_interrupt(22);
        assert(!scan.active && acquisition.faults==0 && queue.head==1);
        assert(ring[0].barcode==183 && ring[0].line_a==173 && ring[0].line_b==right_values[i]);
    }
    puts("ADC0/1/2 ordering and unchanged GP28 raw propagation for low/white/black samples: PASS");
    reset();mock_clock=10800;timer_interrupt(3);
    assert(acquisition.running && scan.active && acquisition.first_fault_reason==IR_REASON_NONE);
    reset();mock_clock=10968;timer_interrupt(3);
    assert(!acquisition.running && acquisition.faults==IR_ACQ_TIMING);
    assert(acquisition.first_fault_reason==IR_REASON_LAUNCH_LATE);
    assert(acquisition.first_fault_us==10968 && acquisition.first_fault_detail==968);
    assert(acquisition.first_fault_samples==12 && wakes==1);
    acquisition_fault(IR_ACQ_OVERFLOW,IR_REASON_OVERFLOW,0);
    assert(acquisition.first_fault_reason==IR_REASON_LAUNCH_LATE); /* first wins */
    assert(acquisition.first_fault_detail==968);
    reset();scan.active=true;scan.started_us=9500;timer_interrupt(3);
    assert(acquisition.first_fault_reason==IR_REASON_SCAN_OVERLAP && acquisition.first_fault_detail==500);
    reset();scan.active=true;scan.started_us=9000;fifo_status=0;adc_interrupt(22);
    assert(acquisition.first_fault_reason==IR_REASON_SCAN_TIMEOUT && acquisition.first_fault_detail==1000);
    reset();scan.active=true;scan.started_us=9900;fifo_status=0;fifo_value=IR_FIFO_ERROR;adc_interrupt(22);
    assert(acquisition.first_fault_reason==IR_REASON_ADC && acquisition.faults==IR_ACQ_ADC);
    reset();scan.active=true;scan.channel=IR_CH_LINE_B;scan.started_us=9900;fifo_status=0;
    queue.head=63;queue.tail=0;adc_interrupt(22);
    assert(acquisition.first_fault_reason==IR_REASON_OVERFLOW && acquisition.faults==IR_ACQ_OVERFLOW);
    reset();scan.deadline_us=UINT32_MAX-200u;mock_clock=100;timer_interrupt(3);
    assert(acquisition.running && acquisition.max_lateness_us==301); /* timer wrap */
    assert(ir_acquisition_start(1)==E_OK);
    assert(acquisition.first_fault_reason==IR_REASON_NONE && acquisition.samples==0);
    puts("IR 800 us boundary, recorded 968 us failure, overlap/scan/ADC/overflow reasons, first-fault latch and restart/wrap: PASS");
}
