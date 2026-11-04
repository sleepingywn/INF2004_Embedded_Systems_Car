#ifndef BARCODE_REPORT_H
#define BARCODE_REPORT_H
#include <stdio.h>
#include "../../../app_program/subsystem2/motion_logic.h"
/* Diagnostic-only: never promotes a rejected capture into a decoded ID. */
static void barcode_report(const bc_msg_t *capture)
{
    char labels[BC_MAX_ELEMS];
    for(unsigned i=0;i<BC_MAX_ELEMS;++i) labels[i]='?';
    puts("N=narrow W=wide ?=unclassified G=character separator; invalid groups are estimates only");
    if(capture->hdr.count==29 && !capture->hdr.overflow) {
        for(unsigned group=0;group<3;++group) {
            unsigned offset=group*10;
            bc_width_t sorted[9];
            for(unsigned i=0;i<9;++i) sorted[i]=capture->widths[offset+i];
            for(unsigned i=1;i<9;++i) {
                bc_width_t value=sorted[i];unsigned j=i;
                while(j && sorted[j-1]>value){sorted[j]=sorted[j-1];--j;}
                sorted[j]=value;
            }
            uint16_t pattern=motion_code39_pattern(capture->widths+offset);
            bool separable=sorted[5]>0 && sorted[5]<sorted[6] && sorted[8]<BC_WIDTH_SAT;
            if(separable)
                for(unsigned i=0;i<9;++i)
                    labels[offset+i]=capture->widths[offset+i]>=sorted[6] ? 'W':'N';
#ifdef BARCODE_RELAXED_WIDTHS
            printf("group%u (%s): ",group+1,pattern ? "rank-only; ratios unchecked":"AMBIGUOUS; estimate");
#else
            printf("group%u (%s): ",group+1,pattern ? "widths valid":"WIDTHS INVALID; estimate");
#endif
            for(unsigned i=0;i<9;++i) printf("%c",labels[offset+i]);
            printf("; narrow max=%u wide min=%u ms\n",(unsigned)sorted[5],(unsigned)sorted[6]);
        }
        labels[9]=labels[19]='G';
    } else puts("Partial/overflow capture: character alignment unknown; no N/W assignment");
    printf("sequence:");
    for(unsigned i=0;i<capture->hdr.count;++i) {
        printf("%s%c:%c(%ums)",i ? " -> ":" ",i%2 ? 'S':'B',labels[i],
               (unsigned)capture->widths[i]);
    }
    puts("");
    printf("black_bars:");
    for(unsigned i=0;i<capture->hdr.count;i+=2) printf(" %c(%u)",labels[i],(unsigned)capture->widths[i]);
    puts("");
    printf("white_gaps:");
    for(unsigned i=1;i<capture->hdr.count;i+=2) printf(" %c(%u)",labels[i],(unsigned)capture->widths[i]);
    puts("");
}
#endif
