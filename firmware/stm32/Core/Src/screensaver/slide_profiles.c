#include "slide_profiles.h"
#include "nfc.h"

SlideProfile_t slide_profiles[] = {
//    { "slide1.bin", "google.com", 0x04 },
//    { "slide2.bin", "yandex.ru",  0x04 },
	{"slide3.bin", "", 				0x04}
};

const uint8_t NUM_SLIDES = sizeof(slide_profiles) / sizeof(slide_profiles[0]);

/* Записать в NFC ссылку текущего слайда */
void Slide_WriteNFC(uint8_t slide_idx) {
    if (slide_idx >= NUM_SLIDES) return;
    NFC_WriteURL_Public(slide_profiles[slide_idx].url,
                        slide_profiles[slide_idx].prefix);
}
