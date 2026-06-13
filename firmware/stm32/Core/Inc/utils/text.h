#ifndef TEXT_H
#define TEXT_H

#include "main.h"

uint16_t text_width_mont(const char* str);

void draw_center_text(
    const char* text,
    uint16_t y,
    uint16_t color,
    uint16_t bg
);

void draw_button_text(
    uint16_t x,
    uint16_t y,
    const char* text,
    uint16_t color,
    uint16_t bg
);

#endif
