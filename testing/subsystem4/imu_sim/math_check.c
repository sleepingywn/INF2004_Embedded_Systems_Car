/* math_check.c - compare imu_math.h against the C maths library (laptop only) */
#include "imu_math.h"
#include <math.h>
#include <stdio.h>
int main(void){
    double e_sqrt=0,e_atan=0,e_sin=0;
    for (double x=1e-4; x<10; x*=1.001){ double r=sqrt(x); double e=fabs(imu_sqrtf((float)x)-r)/r; if(e>e_sqrt)e_sqrt=e; }
    for (int i=0;i<=720;i++) for (int j=1;j<=40;j++){ double a=(i-360)*M_PI/360, m=j*0.05;
        double y=m*sin(a), x=m*cos(a); double e=fabs(imu_atan2f((float)y,(float)x)-atan2(y,x));
        if (e>M_PI) e=fabs(e-2*M_PI);
        if (e>e_atan) e_atan=e; }
    for (double x=-10; x<10; x+=0.0007){ double e=fabs(imu_sinf((float)x)-sin(x)); if(e>e_sin)e_sin=e; }
    printf("imu_sqrtf  max relative error %.2e  (limit 1e-6)\n", e_sqrt);
    printf("imu_atan2f max error %.2e rad = %.5f deg  (limit 1e-5 rad)\n", e_atan, e_atan*180/M_PI);
    printf("imu_sinf   max error %.2e  (limit 1e-6)\n", e_sin);
    int ok = e_sqrt<1e-6 && e_atan<1e-5 && e_sin<1e-6;
    printf("%s\n", ok? "PASS":"FAIL"); return !ok;
}
