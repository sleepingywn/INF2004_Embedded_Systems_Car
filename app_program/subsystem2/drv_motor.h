#ifndef DRV_MOTOR_H
#define DRV_MOTOR_H
#include "../app.h"
#include <stdint.h>
/* Private to the motion task. Values are electrical A-to-B permille. */
ER motor_hw_init(void);
void motor_hw_stop(void);
void motor_hw_write(int16_t left, int16_t right);
#endif
