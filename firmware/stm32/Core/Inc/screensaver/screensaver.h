#pragma once
#include <stdint.h>

void Screensaver_Update(void);
void Screensaver_Exit(void);
uint8_t Screensaver_IsActive(void);

void Screensaver_NextSlide(void);
void Screensaver_PrevSlide(void);
