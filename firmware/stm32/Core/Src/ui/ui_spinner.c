#include <stdint.h>
#include "main.h"
#include "config.h"
#include "lcd_s7796u.h"
#include "ui_spinner.h"

static const int8_t spinner_dx[8] = {  0,  20,  28,  20,   0, -20, -28, -20 };
static const int8_t spinner_dy[8] = { -28, -20,   0,  20,  28,  20,   0, -20 };


void draw_spinner_frame(uint8_t frame) {
    uint16_t cx = SCREEN_W / 2;
    uint16_t cy = SCREEN_H / 2;
    for (uint8_t i = 0; i < 8; i++) {
        uint8_t age = (i - frame + 8) % 8;
        uint16_t color;
        switch (age) {
            case 0: color = 0xFFFF; break;
            case 1: color = 0xAD55; break;
            case 2: color = 0x632C; break;
            case 3: color = 0x2965; break;
            default: color = 0x0000; break;
        }
        int16_t x = (int16_t)cx + spinner_dx[i] - 4;
        int16_t y = (int16_t)cy + spinner_dy[i] - 4;
        LCD_FillRect((uint16_t)x, (uint16_t)y, 8, 8, color);
    }
}

void clear_spinner(void) {
    uint16_t cx = SCREEN_W / 2;
    uint16_t cy = SCREEN_H / 2;
    for (uint8_t i = 0; i < 8; i++) {
        int16_t x = (int16_t)cx + spinner_dx[i] - 4;
        int16_t y = (int16_t)cy + spinner_dy[i] - 4;
        LCD_FillRect((uint16_t)x, (uint16_t)y, 8, 8, COLOR_BLACK);
    }
}
