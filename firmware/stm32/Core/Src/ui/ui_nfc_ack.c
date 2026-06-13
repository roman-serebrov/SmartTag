#include "ui_nfc_ack.h"
#include "lcd_s7796u.h"
#include "main.h"
#include <stdlib.h>

#define SCREEN_W      320
#define ACK_TIMEOUT_MS  1500
#define ACK_IGNORE_MS   2000  /* игнорируем GPO после переключения профиля */

/* Центр и радиус иконки — центр экрана */
#define CX  160
#define CY  120
#define R   40

static uint32_t s_ack_time     = 0;
static uint8_t  s_ack_active   = 0;
static uint32_t s_ignore_until = 0;
static uint8_t  s_need_redraw  = 0;

/* Круг Брезенхема заливкой */
static void fill_circle(uint16_t cx, uint16_t cy, uint16_t r, uint16_t color) {
    for (int16_t y = -r; y <= r; y++) {
        int16_t x_len = (int16_t)(r * r - y * y);
        if (x_len < 0) continue;
        /* целочисленный sqrt через поиск */
        int16_t xr = 0;
        while ((xr+1)*(xr+1) <= x_len) xr++;
        LCD_FillRect(cx - xr, cy + y, xr * 2 + 1, 1, color);
    }
}

static void draw_checkmark(void) {
    /* Зелёный круг */
    fill_circle(CX, CY, R, 0x0460);  /* тёмно-зелёный фон */

    /* Граница круга */
    uint16_t green = COLOR_GREEN;
    for (int a = 0; a < 360; a += 2) {
        /* Рисуем точки на окружности через DrawLine (1px) */
        int16_t x1 = CX + (int16_t)((R-1) * __builtin_cosf(a * 3.14159f / 180.0f));
        int16_t y1 = CY + (int16_t)((R-1) * __builtin_sinf(a * 3.14159f / 180.0f));
        LCD_DrawPixel(x1, y1, green);
    }

    /* Галочка — левая короткая часть (снизу-слева вверх) */
    /* Точка 1: низ галочки */
    /* Точка 2: середина */
    /* Точка 3: правый верх */
    uint16_t x1 = CX - 16, y1 = CY + 4;   /* нижняя левая точка */
    uint16_t x2 = CX - 4,  y2 = CY + 16;  /* нижняя средняя точка */
    uint16_t x3 = CX + 18, y3 = CY - 14;  /* верхняя правая точка */

    /* Рисуем толстую галочку — 3 пикселя толщиной */
    for (int8_t d = -1; d <= 1; d++) {
        LCD_DrawLine(x1, y1 + d, x2, y2 + d, green);
        LCD_DrawLine(x2, y2 + d, x3, y3 + d, green);
    }
}



uint8_t UI_NFCAck_NeedsRedraw(void) {
    if (s_need_redraw) {
        s_need_redraw = 0;
        return 1;
    }
    return 0;
}

/* Вызывается из app.c когда переключается профиль */
void UI_NFCAck_SetIgnore(void) {
    s_ignore_until = HAL_GetTick() + ACK_IGNORE_MS;
}

void UI_NFCAck_Update(void) {
    extern volatile uint8_t g_nfc_gpo_fired;

    if (g_nfc_gpo_fired) {
        g_nfc_gpo_fired = 0;
        /* Игнорируем если только что переключили профиль */
        if (HAL_GetTick() < s_ignore_until) return;

        s_ack_time   = HAL_GetTick();
        s_ack_active = 1;
        draw_checkmark();
    }

    if (s_ack_active && (HAL_GetTick() - s_ack_time >= ACK_TIMEOUT_MS)) {
        s_ack_active = 0;
        /* Не чистим сами — сообщаем app.c что нужно перерисовать UI */
        s_need_redraw = 1;
    }
}
