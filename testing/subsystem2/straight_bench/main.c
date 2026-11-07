/* Standalone, operator-triggered straight-line trim test.
 * No IR, RTOS, MQTT, encoders or PID. Never starts motion automatically.
 * Recorded forward signs: M1/left -1, M2/right +1 (motion_config.h).
 * Verify actual direction with wheels lifted before any floor run.
 */
#include <stdio.h>
#include <stdint.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"

#define M1_A 8u
#define M1_B 9u
#define M2_A 10u
#define M2_B 11u
#define PWM_HZ 10000u
#define PWM_COUNTS 1000u
#define SHORT_PULSE_US 500000u
#define LONG_PULSE_US 1000000u
#define EXTENDED_PULSE_US 2000000u
#define REST_US 500000u

static uint slice_m1;
static uint slice_m2;

static void stop_motors(void)
{
    // Both driver inputs LOW; brake/coast behavior depends on the driver.
    // This commands the outputs; it does not measure physical stopping time.
    pwm_set_both_levels(slice_m1, 0, 0);
    pwm_set_both_levels(slice_m2, 0, 0);
}

static bool init_motors(void)
{
    const uint pins[] = {M1_A, M1_B, M2_A, M2_B};
    for (uint i = 0; i < 4; ++i) {
        gpio_init(pins[i]);
        gpio_put(pins[i], 0);
        gpio_set_dir(pins[i], GPIO_OUT);
    }
    slice_m1 = pwm_gpio_to_slice_num(M1_A);
    slice_m2 = pwm_gpio_to_slice_num(M2_A);

    // fPWM = actual system clock / (divider * (TOP + 1)).
    // A modest TOP avoids the invalid <1 divider in the LAB4 example.
    const float divider = (float)clock_get_hz(clk_sys) /
                          ((float)PWM_HZ * (float)PWM_COUNTS);
    if (divider < 1.0f || divider >= 256.0f) {
        return false;  // GPIO outputs remain LOW.
    }
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, divider);
    pwm_config_set_wrap(&config, PWM_COUNTS - 1u);
    pwm_init(slice_m1, &config, false);
    pwm_init(slice_m2, &config, false);
    stop_motors();
    for (uint i = 0; i < 4; ++i) {
        gpio_set_function(pins[i], GPIO_FUNC_PWM);
    }
    pwm_set_enabled(slice_m1, true);
    pwm_set_enabled(slice_m2, true);
    return true;
}

static void drive_motor(uint slice, bool reverse, unsigned duty)
{
    const uint16_t level = (uint16_t)(PWM_COUNTS * duty / 100u);
    // PWM on one input; the other stays LOW. Positive is electrical A->B,
    // not guaranteed physical forward on a mirrored pair of wheel motors.
    pwm_set_both_levels(slice, reverse ? 0 : level, reverse ? level : 0);
}

static void help(unsigned left, unsigned right, unsigned pulse_us)
{
    printf("\nBuddy 2 straight bench: left GP8/9 sign=-1, right GP10/11 sign=+1\n"
           "f = one forward pulse; s/SPACE = stop; h = help (also stops).\n"
           "a/z = left +1/-1 percentage point; k/m = right +1/-1; bounds 10..40%%.\n"
           "1 = 500 ms; 2 = 1000 ms; 3 = 2000 ms; settings apply to next pulse only.\n"
           "Left=%u%% Right=%u%% Duration=%u ms. No automatic sequence.\n",
           left,right,pulse_us/1000u);
}

int main(void)
{
    const bool ready = init_motors();
    stdio_init_all();
    if (!ready) {
        while (true) {
            printf("Invalid PWM divider; motor GPIOs held LOW.\n");
            sleep_ms(1000);
        }
    }
    unsigned left = 20, right = 20;
    unsigned pulse_us = SHORT_PULSE_US;
    bool running = false;
    bool connected_before = false;
    uint64_t stop_at = 0;
    uint64_t allow_start_at = 0;

    while (true) {
        const uint64_t now = time_us_64();
        const bool connected = stdio_usb_connected();
        // Stop before any serial output, including disconnect/timeout logging.
        if (running && (!connected || now >= stop_at)) {
            stop_motors();
            running = false;
            allow_start_at = now + REST_US;
            // Discard queued keystrokes rather than replaying motor requests.
            while (getchar_timeout_us(0) != PICO_ERROR_TIMEOUT) { }
            if (connected) printf("STOP: pulse complete.\n");
        }
        if (connected && !connected_before) help(left,right,pulse_us);
        connected_before = connected;
        if (!connected) {
            sleep_ms(1);
            continue;
        }
        const int c = getchar_timeout_us(0);
        if (c == 's' || c == ' ' || c == 'h' || c == '?') {
            stop_motors();
            running = false;
            allow_start_at = now + REST_US;
            if (c == 'h' || c == '?') help(left,right,pulse_us);
            else printf("STOP: requested.\n");
        } else if (!running && (c=='a' || c=='z' || c=='k' || c=='m' || c=='1' || c=='2' || c=='3')) {
            if(c=='a' && left<40) ++left;
            if(c=='z' && left>10) --left;
            if(c=='k' && right<40) ++right;
            if(c=='m' && right>10) --right;
            if(c=='1') pulse_us=SHORT_PULSE_US;
            if(c=='2') pulse_us=LONG_PULSE_US;
            if(c=='3') pulse_us=EXTENDED_PULSE_US;
            help(left,right,pulse_us);
        } else if (!running && now >= allow_start_at && c=='f') {
            // Print while stopped, then start the pulse deadline.
            printf("RUN forward left=%u%% right=%u%% duration=%ums\n",left,right,pulse_us/1000u);
            stop_at=time_us_64()+pulse_us;
            running=true;
            drive_motor(slice_m1,true,left);
            drive_motor(slice_m2,false,right);
        }
        // Commands during a pulse/rest are ignored, except stop/help above.
        sleep_ms(1);
    }
}
