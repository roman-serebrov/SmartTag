#include "esp.h"
#include "main.h"
#include "nfc.h"
#include "lcd_s7796u.h"
#include "font_montserrat.h"
#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef huart4;

uint8_t g_esp_busy = 0;

static void ESP_Send(const char* str) {
    HAL_UART_Transmit(&huart4, (uint8_t*)str, strlen(str), 500);
}

void ESP_Init(void) {
    HAL_Delay(1000);
}

void ESP_Update(void) {
    if (!__HAL_UART_GET_FLAG(&huart4, UART_FLAG_RXNE)) return;

    uint8_t cmd;
    HAL_UART_Receive(&huart4, &cmd, 1, 1);

    if (cmd == 'G') {
        char buf[200];
        snprintf(buf, sizeof(buf),
            "{\"total\":%lu,\"p0\":%lu,\"p1\":%lu,\"p2\":%lu,\"p3\":%lu,\"p4\":%lu,\"p5\":%lu,\"p6\":%lu,\"p7\":%lu}\n",
            NFC_GetScanCount(),
            NFC_GetProfileCount(0), NFC_GetProfileCount(1),
            NFC_GetProfileCount(2), NFC_GetProfileCount(3),
            NFC_GetProfileCount(4), NFC_GetProfileCount(5),
            NFC_GetProfileCount(6), NFC_GetProfileCount(7));
        ESP_Send(buf);
    }

    if (cmd == 'U') {
        g_esp_busy = 1;

        /* Пропускаем ':' */
        uint8_t skip;
        HAL_UART_Receive(&huart4, &skip, 1, 500);

        /* Читаем строку до \n */
        char rx_buf[256];
        uint16_t idx = 0;
        uint32_t t = HAL_GetTick();
        while (HAL_GetTick() - t < 3000 && idx < 255) {
            uint8_t byte;
            if (HAL_UART_Receive(&huart4, &byte, 1, 100) == HAL_OK) {
                if (byte == '\n') break;
                rx_buf[idx++] = byte;
            }
        }
        rx_buf[idx] = '\0';

        if (idx > 0) {
            LCD_FillRect(0, 0, 320, 30, 0x001F);
            LCD_DrawStringMont(10, 8, rx_buf, 0xFFFF, 0x001F);
        }

        g_esp_busy = 0;
    }
}
