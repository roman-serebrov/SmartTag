#include "screensaver.h"
#include "lcd_s7796u.h"
#include "font_montserrat.h"
#include "config.h"
#include "fatfs.h"
#include "main.h"
#include "nfc.h"
#include "slide_profiles.h"
#include "esp.h"

#define SCREENSAVER_TIMEOUT_MS  10000
#define SLIDE_INTERVAL_MS       5000
#define SCREEN_W                320
#define SCREEN_H                240
#define MAX_SLIDES              2

static uint8_t  s_active        = 0;
static uint32_t s_last_activity = 0;
static uint32_t s_last_slide    = 0;
static uint8_t  s_slide_idx     = 0;

static uint16_t s_line_buf[SCREEN_W];

/* screensaver.c */
static void draw_slide(uint8_t idx) {
    FATFS fs;
    FIL   fil;
    UINT  br;
    char  fname[] = "slide1.bin";
    fname[5] = '0' + idx + 1;  /* меняем только цифру */

    if (f_mount(&fs, "", 1) != FR_OK) return;
    if (f_open(&fil, fname, FA_READ) != FR_OK) {
        f_mount(NULL, "", 0);
        return;
    }

    for (uint16_t y = 0; y < SCREEN_H; y++) {
        f_read(&fil, s_line_buf, SCREEN_W * 2, &br);
        if (br == SCREEN_W * 2)
            LCD_DrawFrameBuffer(0, y, SCREEN_W, 1, s_line_buf);
    }
    f_close(&fil);
    f_mount(NULL, "", 0);
}

void Screensaver_Update(void) {
    uint32_t now = HAL_GetTick();

    if (!s_active) {
        if (now - s_last_activity >= SCREENSAVER_TIMEOUT_MS) {
            s_active     = 1;
            s_slide_idx  = 0;
            s_last_slide = now;
            draw_slide(s_slide_idx);
            if (!g_esp_busy) Slide_WriteNFC(0);
        }
        return;
    }

    if (now - s_last_slide >= SLIDE_INTERVAL_MS) {
        s_last_slide = now;
        s_slide_idx  = (s_slide_idx + 1) % MAX_SLIDES;
        draw_slide(s_slide_idx);
        if (!g_esp_busy && s_slide_idx < NUM_SLIDES) {
            Slide_WriteNFC(s_slide_idx);
        }
    }
}

void Screensaver_Exit(void) {
    s_active        = 0;
    s_last_activity = HAL_GetTick();
}

uint8_t Screensaver_IsActive(void) {
    return s_active;
}


void Screensaver_NextSlide(void) {
    if (!s_active) return;
    s_slide_idx = (s_slide_idx + 1) % MAX_SLIDES;
    s_last_slide = HAL_GetTick();
    draw_slide(s_slide_idx);
    if (!g_esp_busy && s_slide_idx < NUM_SLIDES) {
        Slide_WriteNFC(s_slide_idx);
    }
}

void Screensaver_PrevSlide(void) {
    if (!s_active) return;
    s_slide_idx = (s_slide_idx == 0) ? MAX_SLIDES - 1 : s_slide_idx - 1;
    s_last_slide = HAL_GetTick();
    draw_slide(s_slide_idx);
    if (!g_esp_busy && s_slide_idx < NUM_SLIDES) {
        Slide_WriteNFC(s_slide_idx);
    }
}

