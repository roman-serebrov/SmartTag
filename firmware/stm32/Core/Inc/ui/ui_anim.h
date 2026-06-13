#pragma once
#include <stdint.h>

void UI_DrawAnimated(uint8_t old_sel, uint8_t new_sel);
void draw_arrows(uint8_t sel);
void draw_labels(uint8_t sel);
void draw_dots(uint8_t sel);
void draw_icon_to_buf(uint8_t prof_idx, int16_t offset_x);
void flush_buf(void);
void draw_ui_animated(uint8_t old_sel, uint8_t new_sel);
