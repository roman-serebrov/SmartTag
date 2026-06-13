#pragma once
#include <stdint.h>

#define FONT_HEIGHT 17

typedef struct {
    uint8_t width;
    uint8_t height;
    int8_t  left;
    int8_t  top;
    uint8_t advance;
    const uint8_t* data;
} GlyphInfo;

void LCD_DrawStringMont(uint16_t x, uint16_t y, const char* str, uint16_t fg, uint16_t bg);
