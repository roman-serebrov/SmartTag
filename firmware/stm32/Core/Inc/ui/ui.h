#pragma once
#include <stdint.h>

void UI_DrawFull(uint8_t sel);
void UI_DrawAnimated(uint8_t old_sel, uint8_t new_sel);
void UI_DrawSpinnerFrame(uint8_t frame);
void UI_ClearSpinner(void);
