#ifndef ENCODER_BENCH_MOCK_H
#define ENCODER_BENCH_MOCK_H
#include <stdint.h>
#include <stdbool.h>
typedef unsigned uint;
#define GPIO_OUT 1
#define GPIO_IN 0
#define GPIO_FUNC_PWM 4
#define GPIO_IRQ_EDGE_RISE 8u
#define GPIO_IRQ_EDGE_FALL 4u
#define PICO_ERROR_TIMEOUT (-1)
#define IO_IRQ_BANK0 13
#define clk_sys 0
void gpio_init(uint);void gpio_put(uint,bool);void gpio_set_dir(uint,bool);
void gpio_disable_pulls(uint);void gpio_set_function(uint,uint);
uint32_t gpio_get_all(void);
void gpio_add_raw_irq_handler_masked(uint32_t,void (*)(void));
void gpio_set_irq_enabled(uint,uint32_t,bool);void irq_set_enabled(uint,bool);
uint32_t save_and_disable_interrupts(void);void restore_interrupts(uint32_t);
uint64_t time_us_64(void);void sleep_ms(uint);int getchar_timeout_us(uint);
void stdio_init_all(void);bool stdio_usb_connected(void);
uint32_t clock_get_hz(uint);
typedef struct {unsigned unused;} pwm_config;
uint pwm_gpio_to_slice_num(uint);pwm_config pwm_get_default_config(void);
void pwm_config_set_clkdiv(pwm_config *,float);void pwm_config_set_wrap(pwm_config *,uint16_t);
void pwm_init(uint,const pwm_config *,bool);void pwm_set_enabled(uint,bool);
void pwm_set_both_levels(uint,uint16_t,uint16_t);
typedef struct {volatile uint32_t intr[4];} fake_io_bank_t;
extern fake_io_bank_t *io_bank0_hw;
#endif
