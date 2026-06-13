#ifndef TOUCH_H
#define TOUCH_H

#include "stm32h7xx_hal.h"

uint8_t Touch_Read(uint16_t *x, uint16_t *y);

#endif
