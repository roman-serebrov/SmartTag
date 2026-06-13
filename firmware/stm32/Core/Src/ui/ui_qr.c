#include "ui_qr.h"
#include "profiles.h"
#include "config.h"
#include "lcd_s7796u.h"
#include "qrcodegen.h"
#include "main.h"      // HAL_Delay, HAL_GPIO_ReadPin, GPIOB, GPIO_PIN_4
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

// Forward declaration
static void draw_qr_screen(void);

// Локальные буферы
static uint8_t  qr_code[qrcodegen_BUFFER_LEN_MAX];
static uint8_t  qr_temp[qrcodegen_BUFFER_LEN_MAX];
static char     qr_text[256];
static uint16_t qr_row_buf[320];

static void draw_qr_screen(void) {
    bool ok = qrcodegen_encodeText(
        qr_text, qr_temp, qr_code,
        qrcodegen_Ecc_MEDIUM,
        qrcodegen_VERSION_MIN, 10,
        qrcodegen_Mask_AUTO, true
    );
    if (!ok) return;

    int size  = qrcodegen_getSize(qr_code);
    int scale = (SCREEN_H - 20) / size;
    if (scale < 1) scale = 1;
    if (scale > 8) scale = 8;

    int qr_px = size * scale;
    int ox    = (SCREEN_W - qr_px) / 2;
    int oy    = (SCREEN_H - qr_px) / 2;

    LCD_FillScreen(COLOR_BLACK);
    HAL_Delay(50);

    uint16_t bg = 0x8410;
    uint16_t fg = 0xFFFF;

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < qr_px; x++) {
            int mx = x / scale;
            qr_row_buf[x] = qrcodegen_getModule(qr_code, mx, y) ? fg : bg;
        }
        for (int s = 0; s < scale; s++) {
            LCD_DrawFrameBuffer(ox, oy + y * scale + s, qr_px, 1, qr_row_buf);
        }
        HAL_Delay(1);
    }
}

void QR_Show(uint8_t sel) {
    if (profiles[sel].is_vcard) {
        snprintf(qr_text, sizeof(qr_text),
            "BEGIN:VCARD\nVERSION:3.0\nFN:%s\nTEL:%s\nEMAIL:%s\nEND:VCARD",
            VC_NAME, VC_PHONE, VC_EMAIL);
    } else if (profiles[sel].is_wifi) {
        snprintf(qr_text, sizeof(qr_text), "WIFI:T:WPA;S:%s;P:%s;;", WIFI_SSID, WIFI_PASSWORD);
    } else {
        snprintf(qr_text, sizeof(qr_text), "https://%s", profiles[sel].url);
    }
    draw_qr_screen();

    uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < QR_TIMEOUT_MS) {
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4) == GPIO_PIN_RESET) {
            HAL_Delay(50);
            break;
        }
    }
}
