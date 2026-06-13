#include "ui_rating.h"
#include "lcd_s7796u.h"
#include "font_montserrat.h"
#include <string.h>

/* Цвета */
#define COLOR_BG        0x0841   /* тёмный фон */
#define COLOR_STAR_ON   0xFEA0   /* жёлтый */
#define COLOR_STAR_OFF  0x4208   /* серый */
#define COLOR_BTN       0x0480   /* зелёный */
#define COLOR_BTN_TXT   0xFFFF   /* белый */
#define COLOR_TITLE     0xFFFF   /* белый */

/* Координаты звёзд */
#define STAR_Y          100
#define STAR_SIZE       32
#define STAR_GAP        12
#define NUM_STARS       5

/* Кнопка Отправить */
#define BTN_X           90
#define BTN_Y           175
#define BTN_W           140
#define BTN_H           36

static uint8_t s_score  = 0;   /* текущая оценка 0-5 */
static uint8_t s_done   = 0;   /* флаг завершения */

/* Считаем X центра каждой звезды */
static uint16_t star_cx(uint8_t i) {
    uint16_t total = NUM_STARS * STAR_SIZE + (NUM_STARS - 1) * STAR_GAP;
    uint16_t start = (320 - total) / 2;
    return start + i * (STAR_SIZE + STAR_GAP) + STAR_SIZE / 2;
}

/* Рисуем одну звезду закрашенным прямоугольником (упрощённо) */
static void draw_star(uint8_t idx, uint8_t filled) {
    uint16_t cx = star_cx(idx);
    uint16_t x  = cx - STAR_SIZE / 2;
    uint16_t color = filled ? COLOR_STAR_ON : COLOR_STAR_OFF;

    /* Рисуем звезду из двух треугольников — упрощённо прямоугольником */
    /* Верхний треугольник */
    for (uint16_t row = 0; row < STAR_SIZE / 2; row++) {
        uint16_t w = 2 + row * 2;
        uint16_t xoff = (STAR_SIZE - w) / 2;
        LCD_FillRect(x + xoff, STAR_Y + row, w, 1, color);
    }
    /* Нижний треугольник */
    for (uint16_t row = 0; row < STAR_SIZE / 2; row++) {
        uint16_t w = STAR_SIZE - row * 2;
        uint16_t xoff = (STAR_SIZE - w) / 2;
        LCD_FillRect(x + xoff, STAR_Y + STAR_SIZE / 2 + row, w, 1, color);
    }
}

/* Рисуем все звёзды */
static void draw_stars(void) {
    for (uint8_t i = 0; i < NUM_STARS; i++) {
        draw_star(i, i < s_score);
    }
}

/* Рисуем кнопку */
static void draw_button(void) {
    uint16_t color = (s_score > 0) ? COLOR_BTN : COLOR_STAR_OFF;
    LCD_FillRect(BTN_X, BTN_Y, BTN_W, BTN_H, color);
    LCD_DrawStringMont(BTN_X + 22, BTN_Y + 10, "Отправить", COLOR_BTN_TXT, color);
}

/* Показываем весь экран рейтинга */
void UI_Rating_Show(void) {
    s_score = 0;
    s_done  = 0;

    LCD_FillScreen(COLOR_BG);

    /* Заголовок */
    LCD_DrawStringMont(85, 50, "Оцените нас!", COLOR_TITLE, COLOR_BG);

    /* Подзаголовок */
    LCD_DrawStringMont(50, 70, "Нажмите на звезду", 0xC618, COLOR_BG);

    draw_stars();
    draw_button();
}

/* Обработка касания — возвращает 1 если оценка отправлена */
uint8_t UI_Rating_Update(uint16_t tx, uint16_t ty, uint8_t touched) {
    if (!touched) return 0;

    /* Проверяем касание звезды */
    for (uint8_t i = 0; i < NUM_STARS; i++) {
        uint16_t cx = star_cx(i);
        if (tx >= cx - STAR_SIZE && tx <= cx + STAR_SIZE &&
            ty >= STAR_Y - 5 && ty <= STAR_Y + STAR_SIZE + 5) {
            s_score = i + 1;
            draw_stars();
            draw_button();
            return 0;
        }
    }

    /* Проверяем касание кнопки Отправить */
    if (s_score > 0 &&
        tx >= BTN_X && tx <= BTN_X + BTN_W &&
        ty >= BTN_Y && ty <= BTN_Y + BTN_H) {
        s_done = 1;
        return 1;
    }

    return 0;
}

uint8_t UI_Rating_IsDone(void)  { return s_done; }
uint8_t UI_Rating_GetScore(void) { return s_score; }
void    UI_Rating_Reset(void)   { s_score = 0; s_done = 0; }
