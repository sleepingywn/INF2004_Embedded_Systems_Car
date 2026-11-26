#include "../app_assert.h"
#include "drv_motor.h"
#include "motion_config.h"
#include "../../config/config_device.h"
#include "../../device/i2c/sysdepend/rp2040/i2c_cnf_sysdep.h"

#if USE_PTMR
#error "USE_PTMR conflicts with motor PWM slices 4/5"
#endif
#define MOTOR_PIN_CONFLICT(p) (((p) >= 8) && ((p) <= 11))
#if DEVCNF_USE_IIC && (MOTOR_PIN_CONFLICT(DEVCNF_I2C0_SDA_PIN) ||              \
                       MOTOR_PIN_CONFLICT(DEVCNF_I2C0_SCL_PIN) ||              \
                       MOTOR_PIN_CONFLICT(DEVCNF_I2C1_SDA_PIN) ||              \
                       MOTOR_PIN_CONFLICT(DEVCNF_I2C1_SCL_PIN))
#error "I2C pin configuration conflicts with motor GPIO8..11"
#endif

#define MOTOR_SLICE_LEFT 4u
#define MOTOR_SLICE_RIGHT 5u
APP_STATIC_ASSERT(motor_pwm_pin_allocation,
                  MOTION_LEFT_A == 8u && MOTION_LEFT_B == 9u &&
                      MOTION_RIGHT_A == 10u && MOTION_RIGHT_B == 11u);
#define MOTOR_REG(slice, offset) (PWM_BASE + (slice) * 0x14u + (offset))

#if MOTION_LINE_ROUTE
/* GP22 is PWM slice 3 channel A on the Robo Pico. Divider = clock in MHz,
   giving a 1 MHz counter; 500 ticks per period produces a 2 kHz chirp. */
#define BUZZER_REG(offset) MOTOR_REG(3u, offset)
#define BUZZER_PERIOD_TICKS 500u
void barcode_buzzer_set(bool sounding)
{
    out_w(BUZZER_REG(PWM_CHx_CC), sounding ? BUZZER_PERIOD_TICKS / 2u : 0u);
}
#endif

/* RP2040 WATCHDOG and PSM registers. Startup configured a 1 us tick.
 * RP2040-E1 decrements LOAD twice per tick. Reset PWM and both processors. */
#define MOTOR_WD_CTRL 0x40058000u
#define MOTOR_WD_LOAD 0x40058004u
#define MOTOR_WD_SCRATCH4 0x4005801cu
#define MOTOR_WD_SELECT 0x40010008u
#define MOTOR_WD_RESET_DOMAINS 0x1fffcu
#define MOTOR_WD_ENABLE (1u << 30)
#define MOTOR_WD_LOAD_250_MS 500000u

void motor_watchdog_feed(void)
{
    out_w(MOTOR_WD_LOAD, MOTOR_WD_LOAD_250_MS);
}

void motor_watchdog_arm(void)
{
    out_w(MOTOR_WD_SCRATCH4, 0u); /* boot the normal drive-inhibited image */
    out_w(MOTOR_WD_SELECT, MOTOR_WD_RESET_DOMAINS);
    motor_watchdog_feed();
    set_w(MOTOR_WD_CTRL, MOTOR_WD_ENABLE);
}

void motor_hw_stop(void)
{
    /* Both A and B LOW = brake, Robo Pico datasheet Table 3.
     * A whole CC register write avoids a transient mixed A/B update. */
    out_w(MOTOR_REG(MOTOR_SLICE_LEFT, PWM_CHx_CC), 0);
    out_w(MOTOR_REG(MOTOR_SLICE_RIGHT, PWM_CHx_CC), 0);
}

ER motor_hw_init(void)
{
    const UINT pins[4] = {MOTION_LEFT_A, MOTION_LEFT_B, MOTION_RIGHT_A,
                          MOTION_RIGHT_B};
    const UW divider16 =
        (TMCLK_MHz * 1000000u * 16u) / (MOTION_PWM_HZ * (MOTION_PWM_TOP + 1u));
    for (unsigned i = 0; i < 4u; ++i)
    {
        gpio_set_val(pins[i], 0);
        gpio_set_pin(pins[i], GPIO_MODE_OUT);
    }
    /* BSP startup only releases PWM with USE_PTMR; release without enabling
     * its competing physical-timer driver. Never reset a running PWM block. */
    clr_w(RESETS_RESET, RESETS_RESET_PWM);
    unsigned attempts = 10000u;
    while (((in_w(RESETS_RESET_DONE) & RESETS_RESET_PWM) == 0u) &&
           (attempts > 0u))
    {
        --attempts;
    }
    if ((attempts == 0u) || (divider16 < 16u) || (divider16 > 4095u))
    {
        return E_SYS;
    }
#if MOTION_LINE_ROUTE
    out_w(BUZZER_REG(PWM_CHx_CSR), 0);
    out_w(BUZZER_REG(PWM_CHx_DIV), TMCLK_MHz * 16u);
    out_w(BUZZER_REG(PWM_CHx_TOP), BUZZER_PERIOD_TICKS - 1u);
    out_w(BUZZER_REG(PWM_CHx_CTR), 0);
    barcode_buzzer_set(false);
    pwm_set_pin(22u);
    set_w(BUZZER_REG(PWM_CHx_CSR), PWM_CH_CSR_EN);
#endif
    for (unsigned slice = MOTOR_SLICE_LEFT; slice <= MOTOR_SLICE_RIGHT; ++slice)
    {
        out_w(MOTOR_REG(slice, PWM_CHx_CSR), 0);
        out_w(MOTOR_REG(slice, PWM_CHx_DIV), divider16);
        out_w(MOTOR_REG(slice, PWM_CHx_TOP), MOTION_PWM_TOP);
        out_w(MOTOR_REG(slice, PWM_CHx_CTR), 0);
        out_w(MOTOR_REG(slice, PWM_CHx_CC), 0);
    }
    for (unsigned i = 0; i < 4u; ++i)
    {
        pwm_set_pin(pins[i]);
    }
    set_w(MOTOR_REG(MOTOR_SLICE_LEFT, PWM_CHx_CSR), PWM_CH_CSR_EN);
    set_w(MOTOR_REG(MOTOR_SLICE_RIGHT, PWM_CHx_CSR), PWM_CH_CSR_EN);
    return E_OK;
}

static void write_wheel(unsigned slice, int16_t duty)
{
    uint32_t magnitude = (uint32_t)((duty < 0) ? -(int32_t)duty : duty);
    if (magnitude > 1000u)
    {
        magnitude = 1000u;
    }
    uint32_t level = magnitude * (MOTION_PWM_TOP + 1u) / 1000u;
    out_w(MOTOR_REG(slice, PWM_CHx_CC), (duty < 0) ? level << 16 : level);
}

void motor_hw_write(int16_t left, int16_t right)
{
    write_wheel(MOTOR_SLICE_LEFT, left);
    write_wheel(MOTOR_SLICE_RIGHT, right);
}
