#pragma once
#include <stdint.h>

typedef enum {
    BTN_NONE  = 0,
    BTN_LEFT  = 1,
    BTN_RIGHT = 2
} ButtonEvent_t;

void Buttons_Init(void);
ButtonEvent_t Buttons_Update(void);
