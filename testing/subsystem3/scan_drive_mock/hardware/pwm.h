#ifndef SCAN_HW_MOCK
#define SCAN_HW_MOCK
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
typedef unsigned uint;
typedef int repeating_timer_t;
typedef int pwm_config;
static uint32_t test_us;
static bool button_up=true, timer_ok=true;
static unsigned levels[6][2];
#define GPIO_OUT 1
#define GPIO_IN 0
#define GPIO_FUNC_PWM 4
#define GPIO_IRQ_EDGE_RISE 1
#define GPIO_IRQ_EDGE_FALL 2
static uint32_t save_and_disable_interrupts(void){return 0;}
static void restore_interrupts(uint32_t x){(void)x;}
static bool gpio_get(unsigned p){(void)p;return button_up;}
static uint32_t gpio_get_all(void){return 0;}
static void gpio_init(unsigned x){(void)x;}
static void gpio_set_dir(unsigned x,unsigned y){(void)x;(void)y;}
static void gpio_put(unsigned x,unsigned y){(void)x;(void)y;}
static void gpio_pull_up(unsigned x){(void)x;}
static void gpio_set_function(unsigned x,unsigned y){(void)x;(void)y;}
static void gpio_set_irq_enabled_with_callback(unsigned p,unsigned e,bool b,void (*f)(uint,uint32_t))
{(void)p;(void)e;(void)b;(void)f;}
static void gpio_set_irq_enabled(unsigned p,unsigned e,bool b){(void)p;(void)e;(void)b;}
static pwm_config pwm_get_default_config(void){return 0;}
static void pwm_config_set_wrap(pwm_config *p,unsigned x){(void)p;(void)x;}
static void pwm_config_set_clkdiv(pwm_config *p,float x){(void)p;(void)x;}
static void pwm_init(unsigned x,pwm_config *p,bool b){(void)x;(void)p;(void)b;}
static void pwm_set_both_levels(unsigned s,unsigned a,unsigned b){levels[s][0]=a;levels[s][1]=b;}
static void pwm_set_enabled(unsigned x,bool b){(void)x;(void)b;}
static uint32_t time_us_32(void){return test_us;}
static uint32_t get_absolute_time(void){return test_us;}
static uint32_t to_ms_since_boot(uint32_t t){return t/1000;}
static bool add_repeating_timer_ms(int t,bool (*f)(repeating_timer_t*),void *p,repeating_timer_t *r)
{(void)t;(void)f;(void)p;(void)r;return timer_ok;}
#endif
