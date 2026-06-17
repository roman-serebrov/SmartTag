#include "esp.h"
#include "main.h"
#include "nfc.h"
#include "lcd_s7796u.h"
#include "font_montserrat.h"
#include "profiles.h"
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

extern UART_HandleTypeDef huart4;

uint8_t g_esp_busy = 0;

/* Флаг: профили получены, нужно перезагрузить в главном цикле */
volatile uint8_t g_profiles_ready = 0;

/* Статические буферы для профилей, полученных с сервера */
static char s_names    [NUM_PROFILES][32];
static char s_urls     [NUM_PROFILES][128];
static char s_filenames[NUM_PROFILES][24];

static uint8_t s_received_count = 0;
static uint8_t s_new_profile_count = 0;

uint8_t ESP_GetProfileCount(void) {
    /* До первого обновления возвращаем 0 — пустое устройство, без точек/стрелок.
       После приёма с сервера — реальное число профилей. */
    return s_new_profile_count;
}

static void ESP_Send(const char* str) {
    HAL_UART_Transmit(&huart4, (uint8_t*)str, strlen(str), 500);
}

/* Читаем строку по UART до '\n' или таймаута */
static uint16_t ESP_ReadLine(char* buf, uint16_t maxlen, uint32_t timeout_ms) {
    uint16_t idx = 0;
    uint32_t t = HAL_GetTick();
    while (HAL_GetTick() - t < timeout_ms && idx < maxlen - 1) {
        uint8_t byte;
        if (HAL_UART_Receive(&huart4, &byte, 1, 10) == HAL_OK) {
            if (byte == '\n') break;
            if (byte != '\r') buf[idx++] = byte;
        }
    }
    buf[idx] = '\0';
    return idx;
}

/* Парсим строку idx:name|url|filename|isRating */
static void ESP_ParseProfile(const char* data) {
    int idx = atoi(data);
    if (idx < 0 || idx >= NUM_PROFILES) return;

    const char* p = strchr(data, ':');
    if (!p) return;
    p++;

    char tmp[220];
    strncpy(tmp, p, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    char* token = strtok(tmp, "|");
    if (token) {
        strncpy(s_names[idx], token, sizeof(s_names[idx]) - 1);
        s_names[idx][sizeof(s_names[idx]) - 1] = '\0';
    } else {
        s_names[idx][0] = '\0';
    }

    token = strtok(NULL, "|");
    if (token) {
        strncpy(s_urls[idx], token, sizeof(s_urls[idx]) - 1);
        s_urls[idx][sizeof(s_urls[idx]) - 1] = '\0';
    } else {
        s_urls[idx][0] = '\0';
    }

    token = strtok(NULL, "|");
    if (token) {
        strncpy(s_filenames[idx], token, sizeof(s_filenames[idx]) - 1);
        s_filenames[idx][sizeof(s_filenames[idx]) - 1] = '\0';
    } else {
        s_filenames[idx][0] = '\0';
    }

    token = strtok(NULL, "|");
    uint8_t is_rating = token ? atoi(token) : 0;

    profiles[idx].name      = s_names[idx];
    profiles[idx].url       = s_urls[idx];
    profiles[idx].filename  = s_filenames[idx];
    profiles[idx].icon      = NULL;   /* иконку загрузит load_icons */
    profiles[idx].is_rating = is_rating;
    profiles[idx].is_wifi   = 0;
    profiles[idx].is_vcard  = 0;
    profiles[idx].prefix    = 0x00;   /* сервер шлёт полный URL */

    s_received_count++;
}

void ESP_Init(void) {
    HAL_Delay(1000);
}

void ESP_Update(void) {
    if (!__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) return;

    uint8_t cmd;
    HAL_UART_Receive(&huart4, &cmd, 1, 1);

    /* Статистика */
    if (cmd == 'G') {
        char buf[200];
        snprintf(buf, sizeof(buf),
            "{\"total\":%lu,\"p0\":%lu,\"p1\":%lu,\"p2\":%lu,\"p3\":%lu,"
            "\"p4\":%lu,\"p5\":%lu,\"p6\":%lu,\"p7\":%lu}\n",
            NFC_GetScanCount(),
            NFC_GetProfileCount(0), NFC_GetProfileCount(1),
            NFC_GetProfileCount(2), NFC_GetProfileCount(3),
            NFC_GetProfileCount(4), NFC_GetProfileCount(5),
            NFC_GetProfileCount(6), NFC_GetProfileCount(7));
        ESP_Send(buf);
    }

    /* Текстовое обновление (старая команда) */
    if (cmd == 'U') {
        g_esp_busy = 1;
        uint8_t skip;
        HAL_UART_Receive(&huart4, &skip, 1, 500);

        char rx_buf[256];
        uint16_t len = ESP_ReadLine(rx_buf, sizeof(rx_buf), 3000);
        if (len > 0) {
            LCD_FillRect(0, 0, 320, 30, 0x001F);
            LCD_DrawStringMont(10, 8, rx_buf, 0xFFFF, 0x001F);
        }
        g_esp_busy = 0;
    }

    /* Профили */
    if (cmd == 'P') {
        /* Приём профилей начался — держим screensaver выключенным
           до конца обновления. Снимет флаг App_ReloadProfiles. */
        g_esp_busy = 1;

        uint8_t next;
        HAL_UART_Receive(&huart4, &next, 1, 100);

        if (next == 'D') {
            /* PD:count — финал. Тяжёлую работу здесь НЕ делаем. */
            uint8_t colon;
            HAL_UART_Receive(&huart4, &colon, 1, 100);

            char num_buf[8];
            ESP_ReadLine(num_buf, sizeof(num_buf), 500);
            s_new_profile_count = (uint8_t)atoi(num_buf);

            /* Очищаем неиспользуемые профили */
            for (uint8_t i = s_new_profile_count; i < NUM_PROFILES; i++) {
                s_names[i][0] = '\0';
                s_urls[i][0] = '\0';
                s_filenames[i][0] = '\0';
                profiles[i].name = s_names[i];
                profiles[i].url = s_urls[i];
                profiles[i].filename = s_filenames[i];
                profiles[i].icon = NULL;
                profiles[i].is_rating = 0;
                profiles[i].is_wifi = 0;
                profiles[i].is_vcard = 0;
            }

            s_received_count = 0;
            /* Только ставим флаг — обработает App_Update в главном цикле */
            g_profiles_ready = 1;

        } else if (next == ':') {
            char line_buf[220];
            uint16_t len = ESP_ReadLine(line_buf, sizeof(line_buf), 2000);
            if (len > 0) {
                ESP_ParseProfile(line_buf);
            }
        }
    }
}
