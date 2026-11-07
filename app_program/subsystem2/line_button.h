#ifndef MOTION_LINE_BUTTON_H
#define MOTION_LINE_BUTTON_H
#include <stdbool.h>
#include <stdint.h>
typedef enum { LINE_BUTTON_NONE, LINE_BUTTON_START, LINE_BUTTON_STOP } line_button_event_t;
typedef struct {
    uint32_t changed_ms;
    bool armed, ready, pressed_once, raw_pressed, started;
} line_button_t;
static inline void line_button_arm(line_button_t *b, uint32_t now, bool pressed)
{
    *b=(line_button_t){.changed_ms=now,.armed=true,.raw_pressed=pressed};
}
static inline line_button_event_t line_button_step(line_button_t *b,uint32_t now,bool pressed)
{
    /* STOP has no debounce delay. A held/bouncing button cannot restart. */
    if(b->started && pressed) {
        *b=(line_button_t){0};return LINE_BUTTON_STOP;
    }
    if(!b->armed)return LINE_BUTTON_NONE;
    if(pressed!=b->raw_pressed){b->raw_pressed=pressed;b->changed_ms=now;}
    if((uint32_t)(now-b->changed_ms)<30u)return LINE_BUTTON_NONE;
    if(!b->ready) {if(!pressed)b->ready=true;return LINE_BUTTON_NONE;}
    if(pressed)b->pressed_once=true;
    else if(b->pressed_once) {
        b->armed=false;b->started=true;return LINE_BUTTON_START;
    }
    return LINE_BUTTON_NONE;
}
#endif
