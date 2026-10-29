/*
 * ir_test.c - Buddy 3 bench test: do the IR sensors tell white from black?
 *
 * Wiring (Buddy3_IR_LineTracing_Barcode.md, section 2; analog A0 mode, D0 unconnected):
 *   IR Left   (barcode)       A0 -> GP26 / ADC0
 *   IR Centre (line sensor A) A0 -> GP27 / ADC1
 *   IR Right  (line sensor B) A0 -> GP28 / ADC2   (Robo Pico VBAT-sense jumper must be OPEN)
 *   VCC -> 3V3, GND -> GND
 *
 * What it does:
 *   Every 100 ms it reads each channel (16-sample average) and prints the raw
 *   12-bit value, the voltage, and the running min/max for that channel.
 *   Once a channel has seen enough contrast (max - min >= MIN_CONTRAST) it
 *   also prints WHITE or BLACK, using the midpoint of min/max as threshold.
 *   It works whichever way the module's polarity is (higher on black or on white):
 *   it learns which end is black from the first surface you call "white" (see below).
 *
 * How to use:
 *   1. Hold every sensor over the WHITE background when the program starts
 *      (the first second of readings is taken as "white" for polarity).
 *   2. Then move them over the black line and back a few times.
 *   3. Press the 'r' key in the serial monitor to reset min/max and relearn.
 *
 * A channel with nothing connected floats and gives random values; ignore it.
 */
#include <stdio.h>
#include <stdbool.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"

#define NUM_CH         3
#define SAMPLES_AVG    16      /* readings averaged per print, smooths noise   */
#define PRINT_MS       100     /* print rate: 10 lines per second               */
#define WHITE_LEARN_MS 1000    /* first 1 s after start/reset = white reference  */
#define MIN_CONTRAST   600     /* counts; below this we refuse to classify (TBC) */

static const uint     ch_gpio[NUM_CH] = { 26, 27, 28 };
static const char    *ch_name[NUM_CH] = { "BC(GP26)", "A (GP27)", "B (GP28)" };

typedef struct {
    uint16_t min, max;
    uint32_t white_sum;   /* sum of readings during the white-learning window */
    uint32_t white_n;
} ch_stats_t;

static ch_stats_t stats[NUM_CH];

static void stats_reset(void)
{
    for (int i = 0; i < NUM_CH; i++) {
        stats[i].min = 4095;
        stats[i].max = 0;
        stats[i].white_sum = 0;
        stats[i].white_n = 0;
    }
}

static uint16_t read_avg(uint ch)
{
    adc_select_input(ch);
    (void)adc_read();                 /* discard first conversion after switching channel */
    uint32_t sum = 0;
    for (int i = 0; i < SAMPLES_AVG; i++) sum += adc_read();
    return (uint16_t)(sum / SAMPLES_AVG);
}

int main(void)
{
    stdio_init_all();

    adc_init();
    for (int i = 0; i < NUM_CH; i++) adc_gpio_init(ch_gpio[i]);  /* disables digital, no pulls */

    /* Give you a few seconds to open the serial monitor before the first line. */
    for (int i = 5; i > 0; i--) {
        printf("IR test starting in %d s - hold sensors over WHITE...\n", i);
        sleep_ms(1000);
    }

    stats_reset();
    absolute_time_t learn_until = make_timeout_time_ms(WHITE_LEARN_MS);
    printf("Learning WHITE for %d ms. Press 'r' to reset.\n", WHITE_LEARN_MS);

    while (true) {
        int c = getchar_timeout_us(0);
        if (c == 'r' || c == 'R') {
            stats_reset();
            learn_until = make_timeout_time_ms(WHITE_LEARN_MS);
            printf("\n--- reset: hold sensors over WHITE, learning for %d ms ---\n", WHITE_LEARN_MS);
        }
        bool learning = !time_reached(learn_until);

        for (int i = 0; i < NUM_CH; i++) {
            uint16_t raw = read_avg((uint)i);
            ch_stats_t *s = &stats[i];
            if (raw < s->min) s->min = raw;
            if (raw > s->max) s->max = raw;
            if (learning) { s->white_sum += raw; s->white_n++; }

            const char *label = "learning";
            if (!learning) {
                uint16_t contrast = s->max - s->min;
                if (contrast < MIN_CONTRAST || s->white_n == 0) {
                    label = "need black";            /* haven't seen both surfaces yet */
                } else {
                    uint16_t mid   = (uint16_t)((s->min + s->max) / 2);
                    uint16_t white = (uint16_t)(s->white_sum / s->white_n);
                    bool white_is_high = white > mid;
                    bool above = raw > mid;
                    label = (above == white_is_high) ? "WHITE" : "BLACK";
                }
            }
            float volts = raw * 3.3f / 4095.0f;
            printf("%s raw=%4u %.2fV [%4u..%4u] %-10s | ",
                   ch_name[i], raw, volts, s->min, s->max, label);
        }
        printf("\n");
        sleep_ms(PRINT_MS);
    }
}
