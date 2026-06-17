/* Core/Src/app/profiles.c
 *
 * ТОЛЬКО таблица профилей. Буфер иконок (icon_data) живёт ИСКЛЮЧИТЕЛЬНО
 * в storage/icons.c. Раньше здесь была вторая копия icon_data[][] на 180 КБ —
 * удалена, это была половина проблемы с памятью.
 *
 * Поле icon у всех профилей инициализируется в NULL: его заполнит
 * load_icons_with_spinner() из storage/icons.c. ui_anim проверяет NULL,
 * так что до загрузки иконок их просто не рисует — без краша.
 */
#include "profiles.h"
#include "config.h"
#include <stdint.h>
#include <stddef.h>   /* NULL */

/* name, url, prefix, color, filename, icon, is_wifi, is_vcard, is_rating */
Profile_t profiles[NUM_PROFILES] = {
    { "Контакт",   "",                                                                     0x00, 0x07FF, "icon_vc.bin",   NULL, 0, 1, 0 },
    { "Вконтакте", "vk.com/id312912508",                                                   0x04, 0x039D, "icon_vk.bin",   NULL, 0, 0, 0 },
    { "Telegram",  "t.me/roman_serebrov",                                                  0x04, 0x055F, "icon_tg.bin",   NULL, 0, 0, 0 },
    { "Instagram", "instagram.com/",                                                       0x04, 0xC8B5, "icon_ig.bin",   NULL, 0, 0, 0 },
    { "YouTube",   "youtube.com/",                                                         0x04, 0xF800, "icon_yt.bin",   NULL, 0, 0, 0 },
    { "Яндекс",    "yandex.ru/maps/org/yandeks/1124715036/?ll=37.587093%2C55.733974&z=16", 0x04, 0xF800, "icon_yan.bin",  NULL, 0, 0, 0 },
    { "Отзывы",    "reviews.yandex.ru/",                                                   0x04, 0x4208, "icon_gh.bin",   NULL, 0, 0, 0 },
    { "Wi-Fi",     "",                                                                     0x00, 0x07FF, "wifi.bin",      NULL, 1, 0, 0 },
    { "Портфолио", "drive.google.com/file/d/1NgbHXSoDwXibVDFyosuSNepRVZSaKFKY/view",       0x04, 0x07E0, "icon_dl.bin",   NULL, 0, 0, 0 },
    { "Рейтинг",   "",                                                                     0x00, 0xFEA0, "icon_star.bin", NULL, 0, 0, 1 },
};
