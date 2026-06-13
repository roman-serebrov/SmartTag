#include <stdint.h>
#include "icons.h"
#include "profiles.h"
#include "config.h"
#include "main.h"
#include "fatfs.h"
#include "ui_spinner.h"   // или ui.h — где живёт draw_spinner_frame
static uint16_t icon_data[NUM_PROFILES][ICON_SIZE * ICON_SIZE];

void load_icons_with_spinner(void) {
    FATFS fs;
    FIL fil;
    UINT br;
    uint8_t frame = 0;

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_SET);

    if (f_mount(&fs, "", 1) != FR_OK) {
        for (uint8_t s = 0; s < 20; s++) {
            draw_spinner_frame(frame++ % 8);
            HAL_Delay(80);
        }
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
        return;
    }

    for (uint8_t i = 0; i < NUM_PROFILES; i++) {
        draw_spinner_frame(frame++ % 8);
        HAL_Delay(60);
        if (profiles[i].filename[0] != '\0') {
            if (f_open(&fil, profiles[i].filename, FA_READ) == FR_OK) {
                f_read(&fil, icon_data[i], ICON_SIZE * ICON_SIZE * 2, &br);
                f_close(&fil);
                if (br == ICON_SIZE * ICON_SIZE * 2)
                    profiles[i].icon = icon_data[i];
            }
        }
        draw_spinner_frame(frame++ % 8);
        HAL_Delay(60);
    }

    while (frame % 8 != 0) {
        draw_spinner_frame(frame++ % 8);
        HAL_Delay(60);
    }

    clear_spinner();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
}
