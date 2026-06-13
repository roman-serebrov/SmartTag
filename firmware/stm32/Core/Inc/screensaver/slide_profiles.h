#pragma once
#include <stdint.h>

typedef struct {
    const char* filename;  /* файл слайда на SD карте */
    const char* url;       /* ссылка для NFC */
    uint8_t     prefix;    /* префикс URL (0x04 = https://) */
} SlideProfile_t;

extern SlideProfile_t slide_profiles[];
extern const uint8_t  NUM_SLIDES;

void Slide_WriteNFC(uint8_t slide_idx);
