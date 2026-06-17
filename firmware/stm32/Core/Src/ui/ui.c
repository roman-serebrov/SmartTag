#include "ui.h"
#include "ui_anim.h"
#include "profiles.h"
#include "config.h"
#include "lcd_s7796u.h"
#include "font_montserrat.h"
#include "text.h"
#include "esp.h"          /* ESP_GetProfileCount() — реальное число профилей */
#include <stdint.h>

// Forward declarations
void draw_arrows(uint8_t sel);
void draw_dots(uint8_t sel);

void draw_labels(uint8_t sel) {
    LCD_FillRect(0, ICON_AREA_Y + ICON_SIZE + 5, SCREEN_W, 55, COLOR_BLACK);
    const char* name = profiles[sel].name;
    uint16_t nw = text_width_mont(name);
    LCD_DrawStringMont((SCREEN_W - nw) / 2, ICON_AREA_Y + ICON_SIZE + 10,
                       (char*)name, COLOR_WHITE, COLOR_BLACK);

    /* QR-кнопку рисуем только когда есть профили */
    if (ESP_GetProfileCount() > 0) {
        LCD_FillRect(256, 195, 48, 28, 0x1F3F);
        LCD_DrawString(264, 202, "QR", COLOR_WHITE, 0x1F3F, 2);
    }

    draw_dots(sel);
}

void UI_DrawFull(uint8_t sel) {
    LCD_FillScreen(COLOR_BLACK);
    draw_arrows(sel);
    draw_icon_to_buf(sel, 0);
    flush_buf();
    draw_labels(sel);
}

void draw_arrows(uint8_t sel) {
    uint8_t count = ESP_GetProfileCount();

    /* Очищаем зоны стрелок */
    LCD_FillRect(0, SCREEN_H/2 - 20, 45, 40, COLOR_BLACK);
    LCD_FillRect(SCREEN_W - 45, SCREEN_H/2 - 20, 45, 40, COLOR_BLACK);

    /* Левая стрелка — только если есть профиль слева */
    if (sel > 0) {
        LCD_DrawString(8, SCREEN_H/2 - 16, "<", COLOR_WHITE, COLOR_BLACK, 4);
    }

    /* Правая стрелка — только если есть профиль справа */
    if (count > 0 && sel + 1 < count) {
        LCD_DrawString(SCREEN_W - 36, SCREEN_H/2 - 16, ">", COLOR_WHITE, COLOR_BLACK, 4);
    }
}

void draw_dots(uint8_t sel) {
    uint8_t count = ESP_GetProfileCount();

    /* Очищаем зону точек */
    LCD_FillRect(0, SCREEN_H - 18, SCREEN_W, 18, COLOR_BLACK);

    /* Если профиль один (или их нет) — точки не нужны */
    if (count <= 1) return;

    uint16_t dot_start = (SCREEN_W - count * 14) / 2;
    for (uint8_t i = 0; i < count; i++) {
        uint8_t size = (i == sel) ? 8 : 5;
        uint16_t dot_x = dot_start + i * 14;
        uint16_t dot_y = SCREEN_H - 14 + (8 - size) / 2;
        uint16_t dot_color = (i == sel) ? COLOR_WHITE : 0x4208;
        LCD_FillRect(dot_x, dot_y, size, size, dot_color);
    }
}
