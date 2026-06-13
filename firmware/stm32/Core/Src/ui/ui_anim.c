#include "ui.h"
#include "profiles.h"
#include "config.h"
#include "lcd_s7796u.h"
#include <string.h>
#include <stdint.h>
#include "ui_anim.h"
// Forward declarations
static void buf_pixel(int16_t x, int16_t y, uint16_t color);
 void flush_buf(void);
 void draw_icon_to_buf(uint8_t prof_idx, int16_t offset_x);

// Буфер — живёт здесь
static uint16_t icon_buf[ICON_AREA_W * ICON_AREA_H];

void draw_ui_animated(uint8_t old_sel, uint8_t new_sel) {
    int8_t dir = (new_sel > old_sel) ? -1 : 1;
    uint8_t steps = 12;

    draw_dots(new_sel);

    for (uint8_t s = 1; s <= steps; s++) {
        int16_t offset = dir * s * (ICON_SIZE / steps);
        memset(icon_buf, 0, sizeof(icon_buf));

        if (profiles[old_sel].icon != NULL)
            for (uint16_t dy = 0; dy < ICON_SIZE; dy++)
                for (uint16_t dx = 0; dx < ICON_SIZE; dx++) {
                    uint16_t c = profiles[old_sel].icon[dy * ICON_SIZE + dx];
                    buf_pixel(ICON_AREA_X + (int16_t)dx + offset, ICON_AREA_Y + dy, c);
                }

        int16_t new_start  = dir * (-(int16_t)ICON_SIZE - 20);
        int16_t new_offset = new_start + (-dir) * s * (ICON_SIZE / steps);
        if (profiles[new_sel].icon != NULL)
            for (uint16_t dy = 0; dy < ICON_SIZE; dy++)
                for (uint16_t dx = 0; dx < ICON_SIZE; dx++) {
                    uint16_t c = profiles[new_sel].icon[dy * ICON_SIZE + dx];
                    buf_pixel(ICON_AREA_X + (int16_t)dx + new_offset, ICON_AREA_Y + dy, c);
                }
        flush_buf();
    }

    draw_icon_to_buf(new_sel, 0);
    flush_buf();
    draw_arrows(new_sel);
    draw_labels(new_sel);
}
void draw_icon_to_buf(uint8_t prof_idx, int16_t offset_x) {
    memset(icon_buf, 0, sizeof(icon_buf));
    if (prof_idx >= NUM_PROFILES) return;
    const Profile_t* p = &profiles[prof_idx];
    if (p->icon == NULL) return;
    for (uint16_t dy = 0; dy < ICON_SIZE; dy++)
        for (uint16_t dx = 0; dx < ICON_SIZE; dx++) {
            uint16_t c = p->icon[dy * ICON_SIZE + dx];
            buf_pixel(ICON_AREA_X + (int16_t)dx + offset_x, ICON_AREA_Y + dy, c);
        }
}

static void buf_pixel(int16_t x, int16_t y, uint16_t color) {
    int16_t bx = x - ICON_AREA_X;
    int16_t by = y - ICON_AREA_Y;
    if (bx < 0 || bx >= ICON_AREA_W || by < 0 || by >= ICON_AREA_H) return;
    icon_buf[by * ICON_AREA_W + bx] = color;
}

 void flush_buf(void) {
    LCD_DrawFrameBuffer(ICON_AREA_X, ICON_AREA_Y, ICON_AREA_W, ICON_AREA_H, icon_buf);
}
