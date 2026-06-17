#include "app.h"
#include "main.h"
#include "nfc.h"
#include "ui.h"
#include "ui_anim.h"
#include "ui_qr.h"
#include "ui_rating.h"
#include "lcd_s7796u.h"
#include "touch.h"
#include "screensaver.h"
#include "profiles.h"
#include <stdint.h>
#include "config.h"
#include "icons.h"
#include "esp.h"
#include "buttons.h"
#include <stdio.h>
#include <string.h>
extern RTC_HandleTypeDef hrtc;

static uint8_t  selected       = 0;
static uint32_t touch_debounce = 0;
static uint8_t  in_rating      = 0;

void App_Init(void) {
    LCD_Init();
    LCD_FillScreen(COLOR_BLACK);
    load_icons_with_spinner();

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
    HAL_Delay(20);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
    HAL_Delay(200);

    NFC_WriteProfile(selected);
    UI_DrawFull(selected);

    /* Подсветку включаем ОДИН раз, после первой отрисовки — так не виден
       мусор из видеопамяти при старте. Дальше PB0 никто не трогает. */
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);

    Screensaver_Exit();
    ESP_Init();
}

/* Перезагрузка профилей после получения с сервера.
   Вызывается из главного цикла, НЕ из обработчика UART —
   здесь стек чистый и блокирующие операции безопасны. */
static void App_ReloadProfiles(void) {
    load_icons_with_spinner();   /* перечитать иконки с SD под новые профили */

    selected  = 0;
    in_rating = 0;

    if (profiles[selected].is_rating) {
        in_rating = 1;
        UI_DrawFull(selected);
        UI_Rating_Show();
    } else {
        NFC_WriteProfile(selected);
        UI_DrawFull(selected);
    }

    Screensaver_Exit();
    g_esp_busy = 0;              /* всё обновление закончено, снимаем busy */
}

static void handle_profile_switch(int8_t direction) {
    uint8_t count = ESP_GetProfileCount();

    if (direction < 0 && selected > 0) {
        uint8_t old = selected--;
        draw_ui_animated(old, selected);
    } else if (direction > 0 && selected + 1 < count) {
        uint8_t old = selected++;
        draw_ui_animated(old, selected);
    } else {
        return;
    }

    if (profiles[selected].is_rating) {
        in_rating = 1;
        Screensaver_Exit();
        UI_Rating_Show();
    } else {
        NFC_WriteProfile(selected);
    }
}

void App_Update(void) {
    /* Обрабатываем флаг получения профилей в главном цикле */
    if (g_profiles_ready) {
        g_profiles_ready = 0;
        App_ReloadProfiles();
        return;
    }

    /* Screensaver не обновляем пока открыт рейтинг или идёт приём */
    if (!in_rating && !g_esp_busy) {
        Screensaver_Update();
    }
    ESP_Update();

    ButtonEvent_t btn = Buttons_Update();
    if (btn != BTN_NONE) {
        if (Screensaver_IsActive()) {
            if (btn == BTN_LEFT)  Screensaver_PrevSlide();
            if (btn == BTN_RIGHT) Screensaver_NextSlide();
        } else {
            if (btn == BTN_LEFT)  handle_profile_switch(-1);
            if (btn == BTN_RIGHT) handle_profile_switch(+1);
        }
    }

    if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_4) == GPIO_PIN_RESET) {
        uint16_t tx, ty;
        if (Touch_Read(&tx, &ty)) {
            uint32_t now = HAL_GetTick();
            if (now - touch_debounce > 600) {
                touch_debounce = now;
                uint8_t count = ESP_GetProfileCount();

                if (in_rating) {
                    uint8_t sent = UI_Rating_Update(tx, ty, 1);
                    if (sent) {
                        in_rating = 0;
                        selected = 0;
                        NFC_WriteProfile(selected);
                        UI_DrawFull(selected);
                        Screensaver_Exit();
                    }
                    return;
                }

                if (Screensaver_IsActive()) {
                    Screensaver_Exit();
                    NFC_WriteProfile(selected);
                    UI_DrawFull(selected);
                    return;
                }

                Screensaver_Exit();

                /* QR кнопка — только если есть профили */
                if (tx >= 250 && ty >= 190 && ESP_GetProfileCount() > 0) {
                    QR_Show(selected);
                    UI_DrawFull(selected);
                }
                /* Стрелка влево */
                else if (tx < 60 && ty > 40 && ty < 200 && selected > 0) {
                    uint8_t old = selected--;
                    draw_ui_animated(old, selected);
                    if (profiles[selected].is_rating) {
                        in_rating = 1;
                        Screensaver_Exit();
                        UI_Rating_Show();
                    } else {
                        NFC_WriteProfile(selected);
                    }
                }
                /* Стрелка вправо */
                else if (tx > 260 && ty > 40 && ty < 180 && selected + 1 < count) {
                    uint8_t old = selected++;
                    draw_ui_animated(old, selected);
                    if (profiles[selected].is_rating) {
                        in_rating = 1;
                        Screensaver_Exit();
                        UI_Rating_Show();
                    } else {
                        NFC_WriteProfile(selected);
                    }
                }
            }
        }
    }
}
