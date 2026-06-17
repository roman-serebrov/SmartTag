/* Core/Src/storage/icons.c — БОЕВАЯ ВЕРСИЯ
 *
 * Единственный владелец буфера иконок и единственный загрузчик.
 *
 * Итоги отладки, учтённые здесь:
 *  - icon_data объявлен ТОЛЬКО тут (раньше дублировался в profiles.c -> 360 КБ RAM).
 *  - FATFS статическая (file-scope), не на стеке (иначе HardFault при повторе).
 *  - Подсветкой PB0 функция НЕ управляет. Её включает App_Init и держит
 *    включённой. Раньше load_icons в конце ставил PB0=RESET и гасил экран
 *    после обновления — это и был "чёрный экран". Убрано полностью.
 */
#include <stdint.h>
#include <string.h>
#include "icons.h"
#include "profiles.h"
#include "config.h"
#include "main.h"
#include "fatfs.h"
#include "ui_spinner.h"

/* Единственный буфер иконок на весь проект. ~180 КБ в .bss (RAM_D1). */
static uint16_t icon_data[NUM_PROFILES][ICON_SIZE * ICON_SIZE];

/* FATFS должен быть static: FatFS хранит указатель на него после f_mount. */
static FATFS s_fs;

void load_icons_with_spinner(void) {
    FIL  fil;
    UINT br;
    uint8_t frame = 0;

    /* Снимаем предыдущее монтирование (безопасно при повторных вызовах). */
    f_mount(NULL, "", 0);

    if (f_mount(&s_fs, "", 1) != FR_OK) {
        for (uint8_t s = 0; s < 20; s++) {
            draw_spinner_frame(frame++ % 8);
            HAL_Delay(80);
        }
        return;
    }

    for (uint8_t i = 0; i < NUM_PROFILES; i++) {
        draw_spinner_frame(frame++ % 8);
        HAL_Delay(60);

        /* По умолчанию иконки нет — ui_anim проверяет NULL. */
        profiles[i].icon = NULL;

        if (profiles[i].filename != NULL && profiles[i].filename[0] != '\0') {
            if (f_open(&fil, profiles[i].filename, FA_READ) == FR_OK) {
                br = 0;
                f_read(&fil, icon_data[i], ICON_SIZE * ICON_SIZE * 2, &br);
                f_close(&fil);
                if (br == ICON_SIZE * ICON_SIZE * 2) {
                    profiles[i].icon = icon_data[i];
                }
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

    /* Размонтируем — карта в согласованном состоянии до след. вызова. */
    f_mount(NULL, "", 0);
}
