#pragma once
#include <stdint.h>

/* Размер иконки в пикселях (квадрат). Должен совпадать с config.h. */
#ifndef ICON_SIZE
#define ICON_SIZE 96
#endif

/* Загружает иконки профилей с SD-карты в общий буфер и проставляет
 * profiles[i].icon. Реализация — в Core/Src/storage/icons.c.
 *
 * Хардкодных массивов иконок (icon_ig/icon_gh/...) больше нет:
 * они не использовались, занимали ~90 КБ FLASH и дублировали логику.
 * Все иконки берутся с SD. */
void load_icons_with_spinner(void);
